#include <stdio.h>
#include <string.h>

#include "cJSON.h"

static int write_text_file(const char *path, const char *text) {
    FILE *file = fopen(path, "w");
    if (file == NULL) {
        return -1;
    }
    int ok = fputs(text, file) >= 0;
    fclose(file);
    return ok ? 0 : -1;
}

static int read_text_file(const char *path, char *buffer, size_t capacity) {
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return -1;
    }
    size_t n = fread(buffer, 1, capacity - 1, file);
    buffer[n] = '\0';
    fclose(file);
    return (int)n;
}

int main(void) {
    puts("jsondemo: start");

    const char *source =
        "{\"service\":\"webd\",\"port\":80,\"features\":[\"static\",\"lua\",\"json\"]}";
    cJSON *root = cJSON_Parse(source);
    if (root == NULL) {
        puts("jsondemo: parse failed");
        return 1;
    }

    const cJSON *service = cJSON_GetObjectItemCaseSensitive(root, "service");
    const cJSON *port = cJSON_GetObjectItemCaseSensitive(root, "port");
    const cJSON *features = cJSON_GetObjectItemCaseSensitive(root, "features");
    if (!cJSON_IsString(service) || strcmp(service->valuestring, "webd") != 0 ||
        !cJSON_IsNumber(port) || port->valueint != 80 ||
        !cJSON_IsArray(features) || cJSON_GetArraySize(features) != 3) {
        puts("jsondemo: object check failed");
        cJSON_Delete(root);
        return 2;
    }
    puts("jsondemo: parse ok");

    cJSON *generated = cJSON_CreateObject();
    cJSON *routes = cJSON_CreateArray();
    if (generated == NULL || routes == NULL) {
        cJSON_Delete(root);
        cJSON_Delete(generated);
        cJSON_Delete(routes);
        return 3;
    }
    cJSON_AddStringToObject(generated, "name", "srvros");
    cJSON_AddNumberToObject(generated, "workers", 2);
    cJSON_AddItemToArray(routes, cJSON_CreateString("/"));
    cJSON_AddItemToArray(routes, cJSON_CreateString("/status"));
    cJSON_AddItemToObject(generated, "routes", routes);

    char *printed = cJSON_PrintUnformatted(generated);
    if (printed == NULL) {
        cJSON_Delete(root);
        cJSON_Delete(generated);
        return 4;
    }
    if (write_text_file("/fat/jsondemo.json", printed) < 0) {
        puts("jsondemo: write failed");
        cJSON_free(printed);
        cJSON_Delete(root);
        cJSON_Delete(generated);
        return 5;
    }

    char readback[512];
    if (read_text_file("/fat/jsondemo.json", readback, sizeof(readback)) <= 0) {
        puts("jsondemo: readback failed");
        cJSON_free(printed);
        cJSON_Delete(root);
        cJSON_Delete(generated);
        return 6;
    }

    cJSON *roundtrip = cJSON_Parse(readback);
    const cJSON *workers = roundtrip != NULL ? cJSON_GetObjectItemCaseSensitive(roundtrip, "workers") : NULL;
    if (!cJSON_IsNumber(workers) || workers->valueint != 2) {
        puts("jsondemo: roundtrip failed");
        cJSON_Delete(roundtrip);
        cJSON_free(printed);
        cJSON_Delete(root);
        cJSON_Delete(generated);
        return 7;
    }
    puts("jsondemo: roundtrip ok");

    cJSON_Delete(roundtrip);
    cJSON_free(printed);
    cJSON_Delete(root);
    cJSON_Delete(generated);
    puts("jsondemo: ok cJSON 1.7.19");
    return 0;
}
