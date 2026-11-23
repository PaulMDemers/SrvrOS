"""Compare release-bound trial observations and retain their evidence paths."""
import json
import re
from urllib.parse import quote


TRAFFIC = "wifi-desktop-active-remove5-traffic:"
CHECKS = (
    ("slot_disabled", "USB slot disabled"),
    ("keys_erased", "Connection keys erased"),
    ("provider_retired", "Network provider retired"),
    ("dma_retained", "DMA retention recorded"),
    ("journal_complete", "Journal complete"),
    ("workspace_clean", "Workspace clean"),
    ("returned_to_ubuntu", "Returned to Ubuntu"),
    ("ubuntu_default_verified", "Ubuntu default verified"),
    ("recovery_verified", "Recovery backup verified"),
)


def fact(value, evidence, passed=None):
    result = {"value": value, "evidence": str(evidence)}
    if passed is not None:
        result["passed"] = passed
    return result


def numeric_row(raw, prefix, fields):
    """A v1 single-removal receipt occurs at most once in the framed exit log."""
    rows = re.findall(rb"^" + re.escape(prefix.encode()) + rb"([^\r\n]*)\r?\n", raw, re.M)
    if not rows:
        return None
    pattern = rb" version=1" + b"".join(b" " + name.encode() + rb"=(0|[1-9][0-9]{0,19})" for name in fields)
    match = re.fullmatch(pattern, rows[0])
    if len(rows) != 1 or not match:
        raise ValueError(f"Duplicate or malformed {prefix} receipt")
    return dict(zip(fields, map(int, match.groups())))


def removal_observations(raw, path, recipe, recipe_path, expected):
    observations = {}
    seen = set()
    data = numeric_row(raw, "wifi-desktop-replug5-data:", ("epoch", "dhcp", "gateway_ping", "dns"))
    if data:
        for key, label in (("dhcp", "DHCP"), ("gateway_ping", "Gateway ping"), ("dns", "DNS lookup")):
            if data[key] not in (0, 1):
                raise ValueError(f"Invalid {key} result in {path}")
            observations[label] = fact(bool(data[key]), path, bool(data[key]))
    traffic = numeric_row(raw, TRAFFIC, ("replies", "required", "elapsed_ticks"))
    if traffic:
        replies, required = traffic["replies"], traffic["required"]
        if required == 0 or replies > required or (expected is not None and expected != required):
            raise ValueError(f"Gateway reply target mismatch in {path}")
        for key, value in (("gateway_replies", replies), ("elapsed_ticks", traffic["elapsed_ticks"])):
            if key in recipe and (type(recipe[key]) is not int or recipe[key] != value):
                raise ValueError(f"{key} differs between {path} and {recipe_path}")
            seen.add(key)
        observations["Gateway replies before removal"] = fact({"observed": replies, "target": required},
                                                              path, replies == required)
        observations["Traffic elapsed ticks"] = fact(traffic["elapsed_ticks"], path)

    loss = numeric_row(raw, "wifi-desktop-active-remove5-loss:", ("slot", "pending_tx", "pending_rx", "worker_stopped"))
    if loss:
        stopped = loss["worker_stopped"]
        if stopped not in (0, 1):
            raise ValueError(f"Invalid worker_stopped in {path}")
        observations["Network worker stopped"] = fact(bool(stopped), path, bool(stopped))
    close = numeric_row(raw, "wifi-desktop-active-remove5-close:", ("disabled", "keys_erased", "provider_retired", "dma_retained"))
    if close:
        close["slot_disabled"] = close.pop("disabled")
        for key, label in CHECKS:
            if key not in close:
                continue
            value = bool(close[key])
            if close[key] not in (0, 1) or (key in recipe and recipe[key] is not value):
                raise ValueError(f"{key} is invalid or differs between {path} and {recipe_path}")
            observations[label] = fact(value, path, value)
            seen.add(key)

    final = numeric_row(raw, "wifi-desktop-active-remove5-final:", ("complete", "removed", "sustained", "return_safe"))
    if final:
        for key, label in (("sustained", "Sustained traffic verified"), ("removed", "Active removal recorded"),
                           ("return_safe", "Safe return after removal")):
            if final[key] not in (0, 1):
                raise ValueError(f"Invalid {key} result in {path}")
            observations[label] = fact(bool(final[key]), path, bool(final[key]))
    return observations, seen


def observations(report, snapshots, record_path, logs, recipe_path, comparison_path, expected=None):
    raw = snapshots.get("exit.log", b"")
    recorded = report.get("previously_reported", {})
    if not isinstance(recorded, dict):
        raise ValueError("Invalid previously reported results")
    recipe = recorded.get("recipe", {})
    storage = recorded.get("comparison", {})
    if not isinstance(recipe, dict) or not isinstance(storage, dict):
        raise ValueError("Invalid recipe or workspace result object")
    match_evidence = logs / "boot.log" if snapshots else record_path
    observations = {"Session match": fact(report["session_match"]["status"], match_evidence)}
    markers = report.get("observed_receipts", {})
    seen = set()
    if TRAFFIC in markers and report.get("exit_snapshot") in ("verified", "verified_continuation"):
        parts = ("session-a.log", "exit.log") if report["exit_snapshot"] == "verified_continuation" else ("exit.log",)
        for name in parts:
            removal, consumed = removal_observations(snapshots[name], logs / name, recipe, recipe_path, expected)
            if observations.keys() & removal.keys():
                raise ValueError("Repeated active-removal checkpoint across exit segments")
            observations.update(removal)
            seen.update(consumed)

    for key, label in CHECKS:
        if key not in seen and (key in recipe or key in storage):
            value = recipe[key] if key in recipe else storage[key]
            if type(value) is not bool or (key in recipe and key in storage and storage[key] is not value):
                raise ValueError(f"Invalid or conflicting check {key}")
            observations[label] = fact(value, recipe_path if key in recipe else comparison_path, value)
            seen.add(key)

    live_workspace = report.get("workspace_changes")
    if live_workspace is not None:
        preserved = not (live_workspace["changed"] or live_workspace["missing"])
        observations["Prior workspace files preserved"] = fact(preserved, record_path, preserved)
    for key, label in (("ubuntu_default", "Ubuntu default verified"), ("installed_recovery", "Recovery backup verified")):
        if key in report.get("recovery", {}):
            status = report["recovery"][key]
            observations[label] = fact(True, record_path, True) if status == "verified" else fact(status, record_path)
    changes = storage if "changed" in storage and "missing" in storage else recipe
    if "changed" in changes and "missing" in changes:
        changed, missing = changes["changed"], changes["missing"]
        if not isinstance(changed, list) or not isinstance(missing, list):
            raise ValueError("Invalid workspace comparison")
        preserved = not (changed or missing)
        observations["Prior workspace files preserved"] = fact(preserved, comparison_path if changes is storage else recipe_path,
                                                               preserved)
    if "fsck_return" in storage:
        if type(storage["fsck_return"]) is not int:
            raise ValueError("Invalid filesystem check exit code")
        observations["Filesystem check exit code"] = fact(storage["fsck_return"], comparison_path,
                                                           storage["fsck_return"] == 0)
    # Additional recipe fields are measurements. Only known check semantics
    # may turn an equal value into a shared pass (e.g. a true failure flag).
    for key, value in recipe.items():
        if key not in seen and key != "verdict":
            observations["Recipe / " + key] = fact(value, recipe_path)
    for key, present in markers.items():
        path = record_path
        if present:
            if key in snapshots:
                path = logs / key
            else:
                prefix = key.encode()
                for name, content in snapshots.items():
                    if content.startswith(prefix) or b"\n" + prefix in content:
                        path = logs / name
                        break
        observations["Receipt / " + key] = fact("observed" if present else "absent", path)
    if "exit_snapshot" in report:
        observations["Exit snapshot"] = fact(report["exit_snapshot"], logs / "exit.log" if raw else logs)
    if "verdict" in recipe:
        observations["Recorded verdict"] = fact(recipe["verdict"], recipe_path)
    return observations


# This is display order for reported observations, not causal attribution.
STAGES = {
    "Identity": ["Session match"],
    "Desktop startup": ["Recipe / firmware_scale", "Recipe / exact_default", "Recipe / guide_opened"],
    "Display settings": ["Recipe / applied_1x", "Recipe / reverted_2x"],
    "Connectivity": ["DHCP", "Gateway ping", "DNS lookup"],
    "Traffic": ["Gateway replies before removal", "Traffic elapsed ticks", "Sustained traffic verified"],
    "USB removal": ["Active removal recorded", "Network worker stopped", "USB slot disabled", "Connection keys erased",
                    "Network provider retired", "DMA retention recorded", "Safe return after removal"],
    "Workspace": ["Recipe / proof_text_saved", "Recipe / text_reopen_logged", "Prior workspace files preserved",
                  "Workspace clean", "Filesystem check exit code"],
    "Return": ["Exit snapshot", "Journal complete", "Returned to Ubuntu", "Ubuntu default verified",
               "Recovery backup verified"],
}


def align(left, right):
    rows = []
    first_difference = None
    shared_passes = []
    stages = {label: stage for stage, labels in STAGES.items() for label in labels}
    all_labels = dict.fromkeys([*left["observations"], *right["observations"]])
    ordered = [label for label in stages if label in all_labels]
    ordered.extend(label for label in all_labels if label not in stages)
    for label in ordered:
        a = left["observations"].get(label)
        b = right["observations"].get(label)
        if a is None or b is None:
            relation = "one_sided"
        elif type(a["value"]) is not type(b["value"]) or a["value"] != b["value"]:
            relation = "different"
            if first_difference is None:
                first_difference = label
        else:
            relation = "same"
            if a.get("passed") is True and b.get("passed") is True:
                shared_passes.append(label)
        rows.append({"stage": stages.get(label, "Recipe evidence"), "check": label, "a": a, "b": b, "relation": relation})
    return {"format": "srvros-trial-comparison-v1", "runs": [left["identity"], right["identity"]],
            "complete_log_capture": left["complete"] and right["complete"],
            "first_difference": first_difference, "shared_passes": shared_passes, "checks": rows}


def escape(text):
    return str(text).replace("\\", "\\\\").replace("|", "\\|").replace("[", "\\[").replace("]", "\\]").replace("\n", " ")


def link(text, path):
    return f"[{escape(text)}]({quote(str(path), safe='/')})"


def cell(item):
    if item is None:
        return "No observation"
    value = item["value"]
    if isinstance(value, dict) and set(value) == {"observed", "target"}:
        value = f"{value['observed']} of {value['target']}"
    elif type(value) is bool:
        value = "Yes" if value else "No"
    elif not isinstance(value, str):
        value = json.dumps(value, ensure_ascii=False)
    return link(value, item["evidence"])


def markdown(comparison):
    lines = ["# Trial comparison", ""]
    for side, run in zip(("A", "B"), comparison["runs"]):
        lines.append(f"- {side} · {link(run['release'], run['record'])} · {escape(run['model'])} · "
                     f"{escape(run['session'] or run['session_match'])}")
    rows = comparison["checks"]
    first = next((row for row in rows if row["check"] == comparison["first_difference"]), None)
    lines.append("")
    if first:
        lines.append(f"**First differing observation** · {escape(first['check'])} · "
                     f"A {cell(first['a'])} · B {cell(first['b'])}")
    else:
        lines.append("No difference was observed in the checks recorded on both sides." if any(
            row["relation"] == "same" for row in rows) else "No shared observation is available.")
    if not comparison["complete_log_capture"]:
        lines.extend(["", "At least one run has incomplete session or log evidence."])
    if comparison["shared_passes"]:
        lines.extend(["", "**Passed in both** · " + ", ".join(escape(label) for label in comparison["shared_passes"])])
    if rows:
        lines.extend(["", "| Stage | Check | A | B |", "|---|---|---|---|"])
        for row in rows:
            lines.append(f"| {row['stage']} | {escape(row['check'])} | {cell(row['a'])} | {cell(row['b'])} |")
    return "\n".join(lines)
