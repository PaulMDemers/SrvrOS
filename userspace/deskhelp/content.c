#include "content.h"
#include <srvros/desktop_protocol.h>
#include <string.h>
#define T(id,text) {id,DESK_DOCUMENT_TITLE,text}
#define H(id,text) {id,DESK_DOCUMENT_HEADING,text}
#define P(id,text) {id,DESK_DOCUMENT_PARAGRAPH,text}
#define N(id,text) {id,DESK_DOCUMENT_NOTE,text}
#define C(id,text) {id,DESK_DOCUMENT_CODE,text}
static const struct desk_document_block welcome[]={
 T(1,"Welcome to SrvrOS"),P(2,"A small desktop, built from the ground up. Open a tool, arrange your workspace and make something. This guide travels with the system and works without a network connection."),
 H(3,"Find your way"),P(4,"The dock opens Files, Text, Calculator, System, Settings and Terminal. Press Command-Space to search for any application or an open window. Preview, Sketch, Clock and Help are available there too."),
 H(5,"Start with a document"),P(6,"Open Text, type a few lines and choose File > Save. Your home folder is /fat/home. Files can open saved text and images with the appropriate application."),
 H(7,"Make it yours"),P(8,"Settings offers larger text, wallpaper colors and clock preferences. The global menu follows the active application. Click the clock to open the calendar."),
 H(9,"Using this guide"),P(10,"Choose Topics or press Ctrl-L to search the guide. Back and Forward restore the page and your scroll position. Copy page puts a plain-text copy on the shared clipboard. Open app starts the tool described by the current topic."),
 N(11,"Press F1 in an application to open its help topic. For keyboard controls, search Topics for Keyboard.")};
static const struct desk_document_block desktop[]={
 T(1,"Your desktop"),H(2,"Applications and windows"),P(3,"Click a dock icon to open its application or focus an existing window. Command-Space opens the launcher: type a name, use Up or Down to choose a result, then Enter to open it. Escape dismisses the launcher."),
 P(4,"Drag a window by its title bar. Drag its lower-right corner to resize it. The title-bar controls close, minimize and zoom. The global Window menu lists open windows and offers the same window actions."),
 H(5,"Focus and menus"),P(6,"Click a window to focus it, or press Alt-Tab to cycle forward and Alt-Shift-Tab to cycle backward. F10 focuses the menu bar. Use arrows to choose a heading or action, Enter to activate and Escape to dismiss. Right-click or Control-click opens contextual actions; Shift-F10 or the Menu key opens them for the focused control. The desktop background has its own application menu."),
 H(7,"Scrolling"),P(8,"Two-finger trackpad scrolling and the mouse wheel scroll the view beneath the pointer. You can also drag scrollbar thumbs. Inside a focused list or document, use arrows, Page Up, Page Down, Home and End."),
 H(9,"The clock and calendar"),P(10,"Click the clock to browse the calendar. Arrow keys move by day or week, Page Up and Page Down change month, and Ctrl-Home returns to today. Settings controls the clock format, date display and fixed UTC offset."),
 H(11,"Finishing a session"),P(12,"Choose SrvrOS > Return to console. Applications can ask you to save unfinished work before they close. Ctrl-Alt-Escape exits the desktop immediately and should be reserved for a stuck session."),
 H(13,"Screenshots"),P(14,"Command-Shift-3 or SrvrOS > Save screenshot captures the whole desktop, including its pointer. Saving runs in the background. The completion notification opens /fat/home/Screenshots; choose a BMP there to view it in Preview. Capture supports displays whose BMP fits within 8 MiB, including full HD. Return to console waits for a pending save; immediate exit may abandon an incomplete capture."),N(15,"The desktop currently supports eight application windows at once. Close a window if there is no room to open another.")};
static const struct desk_document_block files[]={
 T(1,"Files"),P(2,"Browse folders, open documents and manage individual files. Home opens /fat/home, Volume opens /fat, and Root shows the filesystem root."),
 H(3,"Go somewhere"),P(4,"Press Ctrl-L to edit the location, type an absolute path and press Enter. Alt-Left and Alt-Right move through visited folders; Alt-Up opens the parent; Alt-Home opens Home. Ctrl-R refreshes the listing."),
 H(5,"Open and organize"),P(6,"Select a row, then use Open or press Enter. Folders open in Files; PNG, JPEG and BMP images open in Preview; other files open in Text. New folder, Rename, Duplicate and Delete are available from the controls and menus."),
 P(7,"Duplicate copies an individual file up to 16 MiB and verifies its contents before reporting success. Delete removes an individual file or an empty folder after confirmation. There is no Trash or automatic recovery of deleted files."),
 H(8,"Paths and volumes"),P(9,"A path beginning with / is absolute. /fat is the main exFAT volume in this desktop environment. Other mounted volumes appear at their assigned paths. Files does not format, partition or automatically mount drives."),
 H(11,"Information and paths"),P(12,"Select an item and press Ctrl-I or choose Information from File or its contextual menu. This reads current metadata without loading file contents. Copy path copies its absolute path; Copy details exports the displayed metadata. Ctrl-C copies details and Ctrl-Shift-C copies the path. Escape returns to Files. Close and reopen to refresh; folder contents are not totaled."),
 N(10,"A failed operation leaves a message in the window. Check the destination, free space and whether the volume is writable before trying again.")};
static const struct desk_document_block text[]={
 T(1,"Text"),P(2,"Write and edit plain UTF-8 documents. The editor supports selection, a shared clipboard, search and multiple steps of Undo and Redo."),
 H(3,"Open and save"),P(4,"Ctrl-N creates a document. Ctrl-O opens the file picker. Ctrl-S saves the current document; Ctrl-Shift-S chooses a new path. Closing an edited document asks whether to save, discard or cancel."),
 P(5,"The file picker accepts a folder location and a filename. Saving checks for a file that changed since it was opened and asks before overwriting an existing destination."),
 H(6,"Edit"),P(7,"Ctrl-A selects all; Ctrl-X cuts; Ctrl-C copies; Ctrl-V pastes. Ctrl-Z undoes and Ctrl-Shift-Z redoes. Shift with navigation keys extends the selection. Click and drag to select text. Right-click or Control-click opens editing actions while preserving your selection; Shift-F10 opens the same menu from the keyboard."),
 H(8,"Find text"),P(9,"Ctrl-F opens Find. Ctrl-G moves to the next match and Ctrl-Shift-G to the previous one. The search bar shows the result and offers case-sensitive matching. Escape closes it."),
 H(13,"Replace text"),P(14,"Ctrl-H opens Find and Replace. Enter a literal Find value and a With value; an empty With removes matching text. Replace changes the selected match and finds the next. Replace all changes every nonoverlapping match in the original document as one undo step. Enter in With replaces one; Ctrl-Enter replaces all. A replacement that would exceed the document limit makes no changes. Escape returns to the document; Undo then restores the edit."),
 H(10,"Working limits"),P(11,"Documents are limited to 64 KiB. Undo history is bounded to 256 records and 256 KiB of edit data; the oldest entries are dropped when it fills. Saving keeps Undo history, while opening a different document starts a new history."),
 N(12,"UTF-8 text is preserved, but the current fonts cover a limited set of characters. A replacement glyph means the font cannot display that character; it does not mean the saved text was replaced.")};
static const struct desk_document_block calculator[]={
 T(1,"Calculator"),P(2,"Type an expression or use the keypad. Press Enter or Equals to calculate. Parentheses and the four basic arithmetic operations are supported."),
 H(3,"Continue a calculation"),P(4,"After a result, type an operator to continue with that value or type a digit to begin again. Ans inserts the previous answer. The plus/minus control changes the sign."),
 H(5,"Use a previous result"),P(6,"The session history holds the latest 16 calculations. Select a row and choose Use to bring its result into the expression, or Copy result to put it on the clipboard."),
 H(7,"Precision"),P(8,"Calculations use floating-point numbers and display up to 12 significant digits. The expression field holds up to 255 bytes. Very large or very small values can overflow or underflow; an error leaves the expression available to edit."),
 N(9,"History belongs to this window and is not saved after it closes.")};
static const struct desk_document_block terminal[]={
 T(1,"Terminal"),P(2,"Run command-line tools in a terminal window. Each window owns a separate shell and pseudo-terminal. Resize the window to change the terminal dimensions."),
 H(3,"A few commands"),C(4,"pwd\nls /fat/home\ncat /fat/home/example.txt\nps\ndate"),
 H(5,"Input and clipboard"),P(6,"Ctrl-C interrupts the foreground command. Ctrl-Shift-C copies selected terminal text; Ctrl-Shift-V pastes. Command-C and Command-V are also available. Clipboard text is sent as input to the shell, so review commands before pressing Enter."),
 H(7,"The shell environment"),P(8,"Programs are searched in /fat/bin, / and /fat. HOME is /fat; the desktop document folder is /fat/home. The shell starts at /. Pipes and redirection let you combine tools."),
 C(9,"seq 1 10 | wc -l\necho hello > /fat/home/greeting.txt"),
 H(10,"Closing"),P(11,"Type exit to leave the shell. Closing the window while its shell is running asks for confirmation. This terminal does not yet provide full Unix job control or saved shell history.")};
static const struct desk_document_block preview[]={
 T(1,"Preview"),P(2,"Open PNG, JPEG and BMP images. Choose one in Files or press Ctrl-O in Preview to browse. Decoding runs in a separate worker so the desktop can continue handling input. Ctrl-E or File > Edit in Sketch opens the current image in the drawing tool."),
 H(3,"Look closer"),P(4,"Fit or 0 fits the image inside the window without enlarging a small image. 1:1 or 1 displays one image pixel per screen pixel. Plus and Minus step through zoom levels from 6.25% to 400%."),
 P(5,"Drag to pan a zoomed image. Scrolling and arrow keys move around it; Page Up, Page Down, Home and End offer larger movements. Transparent pixels appear over a checkerboard."),
 H(6,"Opening another image"),P(7,"Cancel or Escape stops an in-progress opening. The previous image remains visible if the new file cannot be opened. Open is available again after the canceled worker has finished closing."),
 H(8,"Supported images"),P(9,"Files may be up to 8 MiB, with at most 4,194,304 pixels and 4096 pixels along either side. Preview displays a still image and does not edit or export it."),
 N(10,"EXIF orientation, color profiles, animation and smooth resampling are not implemented yet. Some photographs may need manual rotation in another tool before opening here.")};
static const struct desk_document_block settings[]={
 T(1,"Settings"),P(2,"Choose a comfortable text size, wallpaper and clock format. Preferences are shared with open applications and saved to /fat/home/desktop.conf."),
 H(3,"Appearance"),P(4,"Standard and Larger adjust the desktop body text. Sage, Sand and Slate change the desktop wallpaper color. Applications reflow their controls where space permits."),
 H(5,"Clock"),P(6,"Choose a 12-hour or 24-hour clock and whether to show the date. The UTC offset changes in 15-minute steps. This is a fixed offset, so daylight-saving changes must be applied manually."),
 H(7,"Saving preferences"),P(8,"The window reports whether changes were saved. If saving fails, the preferences may still be active for this session but will not necessarily survive a restart. Check that the main volume is writable."),
 N(9,"Settings currently covers desktop appearance and the clock. It does not configure storage, networking, accounts or power management.")};
static const struct desk_document_block system[]={
 T(1,"System and network status"),P(2,"System provides a live view of processes and network status. Switch between the Processes and Network tabs. Pause freezes the display; Refresh or F5 takes a new snapshot."),
 H(3,"Processes"),P(4,"The process table lists the running programs and their current state. It is a read-only view; use the command-line tools when a process must be managed."),
 H(5,"Network"),P(6,"The Network tab reports the available interface, link state, MAC and IPv4 addresses, router, DNS configuration and packet counters. A disconnected or unavailable interface is reported explicitly."),
 H(7,"Configuration"),P(8,"Network setup remains a console task. Available tools include ifconfig, route, arp, ping, host and netstat. The virtual machine can exercise the supported emulated adapter; support for a particular USB network adapter depends on its driver."),
 N(9,"An interface being present does not by itself prove that DNS or Internet access works. Check the address and route, then test the destination you need.")};
static const struct desk_document_block keyboard[]={
 T(1,"Keyboard shortcuts"),H(2,"Across the desktop"),C(3,"Command-Space    Applications and windows\nAlt-Tab          Next window\nAlt-Shift-Tab    Previous window\nF10              Global menu bar\nShift-F10        Contextual actions\nF1               Help for the active application\nCommand-Shift-N  Notifications\nCommand-Shift-3  Save screenshot"),
 H(4,"Inside controls"),P(5,"Tab moves to the next control and Shift-Tab moves backward. Space activates a focused button. Arrow keys navigate lists and documents. Page Up and Page Down scroll a page; Home and End move to an edge. Escape dismisses a dialog or chooser."),
 H(6,"Text editing"),C(7,"Ctrl-A           Select all\nCtrl-X / C / V   Cut / Copy / Paste\nCtrl-Z           Undo\nCtrl-Shift-Z     Redo\nCtrl-F           Find in Text\nCtrl-G           Next match\nCtrl-Shift-G     Previous match"),
 H(8,"Files and documents"),C(9,"Ctrl-O           Open\nCtrl-S           Save in Text\nCtrl-Shift-S     Save as in Text\nCtrl-L           Location in Files; Topics in Help\nCtrl-I           Information in Files\nAlt-Left/Right   Back / Forward\nCtrl-W           Close window"),
 H(10,"Terminal"),P(11,"Use Ctrl-Shift-C and Ctrl-Shift-V for its clipboard. Ctrl-C remains the command interrupt key. Command-C and Command-V are also supported."),
 N(12,"On the MacBook keyboard, Command is the Meta modifier. Depending on the keyboard or virtual machine, function keys may require Fn. Application menus show their available shortcuts.")};
static const struct desk_document_block limits[]={
 T(1,"Current scope"),P(2,"SrvrOS is an evolving native operating system and desktop. Its applications share a widget library, event handling, menus, clipboard and file pickers. This guide describes the implemented features, not a compatibility promise for another desktop system."),
 H(3,"Documents and windows"),P(4,"There are eight application-window slots. Text documents are limited to 64 KiB; Files can duplicate individual files up to 16 MiB. The clipboard holds up to 64 KiB. Preview has separate image limits listed in its topic."),
 H(5,"Text and language"),P(6,"The current UI fonts include ASCII, Latin-1 and selected punctuation. UTF-8 bytes are preserved in documents, but complex text shaping, bidirectional layout, input methods and broad Unicode font fallback are not implemented."),
 H(7,"Desktop services"),P(8,"There is no Trash, application installation interface, browser, sound mixer, full account manager or automatic timezone database yet. Networking and additional hardware depend on the available drivers and console tools."),
 H(9,"Testing"),P(10,"Virtual-machine tests exercise application behavior and failure paths. They do not replace checks on the physical laptop, particularly for storage, input timing, power management and device drivers."),
 N(11,"Save work before ending a session. A destructive file operation does not have a desktop undo or recovery facility.")};
static const struct desk_document_block about[]={
 T(1,"SrvrOS"),P(2,"A native operating system and a quiet, practical desktop. Built from scratch, with the MacBook Air as its first physical desktop target."),
 H(3,"One shared toolkit"),P(4,"Applications use the same buttons, text fields, lists, tables, tabs, scrollbars, dialogs, file pickers, image views, property views, document views, sliders, drawing canvases, color panels and contextual menus. The desktop supplies window management, global menus, a launcher, calendar and clipboard."),
 H(5,"Included tools"),P(6,"Files, Text, Calculator, System, Settings, Terminal, Preview, Sketch, Clock and Help. Open app below opens System for a view of running processes and network status."),
 H(7,"Acknowledgments"),P(8,"The desktop uses DejaVu fonts. Preview uses the stb_image decoder under its MIT license. The source repository includes upstream licenses and records local changes."),
 N(9,"This is a development system. Hardware acceptance and virtual-machine validation are tracked separately in the project documentation.")};
static const struct desk_document_block clock_guide[]={
 T(1,"Clock"),P(2,"Keep a countdown timer and a stopwatch in one window. Both use monotonic time, so changing the displayed wall clock or UTC offset does not change their elapsed time."),
 H(3,"Set a timer"),P(4,"Enter a duration as seconds, m:ss or h:mm:ss, from one second to 24 hours. Start begins counting down. Pause and Resume keep the remaining time; Reset stops the timer and restores its duration. Again repeats a completed timer."),
 H(5,"Time a task"),P(6,"Choose Stopwatch, then Start. Lap records the elapsed time and the split since the previous lap. The latest 64 laps are retained. Pause stops accumulation, Resume continues it, and Reset clears a paused stopwatch and its laps."),
 H(7,"Copy and keyboard controls"),P(8,"Copy puts the displayed time on the clipboard. Copy laps in the Clock menu exports a tab-separated table. Ctrl-1 selects Timer, Ctrl-2 selects Stopwatch, Ctrl-Enter starts or pauses, Ctrl-R resets and Ctrl-L records a lap. Ctrl-Shift-C copies time; Ctrl-Shift-L copies laps."),
 H(9,"Work in another window"),P(10,"Minimize Clock to keep timing while you work. A completed timer shows a brief notification without taking focus. Alerts in the menu bar, SrvrOS > Notifications or Command-Shift-N opens the session history. Open app returns to the source window when it is still open; otherwise it starts the application."),
 P(11,"Click a notification's close button to dismiss its banner. The history retains up to 16 notices, with at most four from one window. Clear removes a selected notice and Clear all empties the history. Opening history marks its notices as read."),
 N(12,"Closing Clock stops its timer and stopwatch; the window asks first while either is running. Timers, laps and notifications are not saved across sessions. There is no audible alarm or suspend/resume guarantee. The stopwatch stops at 100 hours.")};
static const struct desk_document_block sketch_guide[]={
 T(1,"Sketch"),P(2,"Draw on a blank 640 by 360 canvas, or open a PNG, JPEG or BMP image. Sketch uses the shared color panel, sliders, canvas and image workers."),
 H(3,"Draw and navigate"),P(4,"Choose Pen, Line or Box, then drag on the canvas. B, L and R select those tools. Pan (H), wheel scrolling and arrow keys move a zoomed canvas. Plus and minus zoom; 0 fits and 1 shows actual pixels. The Size slider sets a 1 to 32 pixel brush. Paint with white to erase."),
 H(5,"Choose a color"),P(6,"Color or Ctrl-K opens the RGB panel. Drag a channel slider, or use arrow keys, Page Up/Down and Home/End. Shift with an arrow changes ten units. Choose a swatch for a preset. Copy puts the hex color on the clipboard. Use applies the draft; Cancel keeps the previous brush color."),
 H(7,"Undo and cancel"),P(8,"Each completed stroke, line, box or clear is one undo step. Ctrl-Z undoes and Ctrl-Shift-Z redoes; the latest four steps are retained. A new edit discards redo. Escape, focus loss or resize cancels an unfinished stroke or shape. Edit > Clear canvas fills white and can be undone."),
 H(9,"Save a copy"),P(10,"Ctrl-S writes a new BMP copy through a background worker and verifies the saved bytes. Choose a filename ending in .bmp. Existing files are kept; use a different name. Drawing can continue during saving, and newer changes remain marked unsaved. New, Open and Close ask what to do with unsaved work. Imports retain their original files."),
 N(11,"Sketch flattens transparent images onto white and saves opaque BMPs. Its image size must fit an 8 MiB BMP; inputs also follow Preview's format limits. There are no layers, text objects, selections, flood fill or pressure-sensitive brushes yet.")};
#define TOPIC(id,alias,title,detail,app,data) {id,alias,title,detail,app,data,sizeof(data)/sizeof(*data)}
const struct help_topic help_topics[]={
 TOPIC(1,"welcome","Welcome","Start here · applications and documents",0,welcome),
 TOPIC(2,"desktop","Your desktop","Windows · launcher · menus · calendar",0,desktop),
 TOPIC(3,"files","Files","Folders · paths · rename · duplicate · delete",DESK_APP_FILES,files),
 TOPIC(4,"text","Text","Documents · save · edit · find · undo",DESK_APP_TEXT,text),
 TOPIC(5,"calculator","Calculator","Expressions · answers · history",DESK_APP_CALCULATOR,calculator),
 TOPIC(6,"terminal","Terminal","Shell · commands · pipes · clipboard",DESK_APP_TERMINAL,terminal),
 TOPIC(7,"preview","Preview","Images · zoom · pan · PNG JPEG BMP",DESK_APP_PREVIEW,preview),
 TOPIC(8,"settings","Settings","Appearance · text size · wallpaper · clock",DESK_APP_SETTINGS,settings),
 TOPIC(9,"system","System and network","Processes · status · interface · DNS",DESK_APP_SYSTEM,system),
 TOPIC(10,"keyboard","Keyboard shortcuts","Navigation · editing · Command and Control",0,keyboard),
 TOPIC(11,"limits","Current scope","Limits · supported features · testing",0,limits),
 TOPIC(12,"about","About SrvrOS","System · toolkit · acknowledgments",DESK_APP_SYSTEM,about),
 TOPIC(13,"clock","Clock","Timer · stopwatch · laps · alerts · notifications",DESK_APP_CLOCK,clock_guide),
 TOPIC(14,"sketch","Sketch","Drawing · brushes · color · BMP copies",DESK_APP_SKETCH,sketch_guide),
};
const unsigned help_topic_count=sizeof(help_topics)/sizeof(*help_topics);
const struct help_topic *help_topic(unsigned id) { for(unsigned i=0;i<help_topic_count;i++) if(help_topics[i].id==id) return &help_topics[i];return NULL; }
const struct help_topic *help_topic_alias(const char *alias) { if(alias) for(unsigned i=0;i<help_topic_count;i++) if(!strcmp(help_topics[i].alias,alias)) return &help_topics[i];return &help_topics[0]; }
