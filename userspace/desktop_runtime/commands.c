#include <srvros/desk_commands.h>
#include <string.h>
static unsigned modifiers(unsigned m) {
    unsigned result=0;
    if(m&SRV_KEY_SHIFT)result|=SRV_KEY_SHIFT;
    if(m&SRV_KEY_ALT)result|=SRV_KEY_ALT;
    if(m&(SRV_KEY_CTRL|SRV_KEY_META))result|=SRV_KEY_CTRL;
    return result;
}
int desk_commands_enabled(const struct desk_command *commands,unsigned count,unsigned id) {
    if(!commands || !id)return 0;
    const struct desk_command *found=NULL;
    for(unsigned i=0;i<count;i++)if(commands[i].id==id) {if(found)return 0;found=&commands[i];}
    return found && !(found->flags&DESK_MENU_DISABLED);
}
unsigned desk_commands_key(const struct desk_command *commands,unsigned count,const struct srv_key_event *key) {
    if(!commands || !key || !(key->flags&SRV_KEY_DOWN_EVENT) || (key->flags&(SRV_KEY_CANCEL|SRV_KEY_REPEAT)))return 0;
    unsigned found=0;
    for(unsigned i=0;i<count;i++)if(commands[i].usage && commands[i].usage==key->usage &&
       modifiers(commands[i].modifiers)==modifiers(key->modifiers) && desk_commands_enabled(commands,count,commands[i].id)) {
        if(found)return 0;
        found=commands[i].id;
    }
    return found;
}
int desk_commands_invoke(const struct desk_command *commands,unsigned count,unsigned id,
                         void (*invoke)(void *,unsigned),void *context) {
    if(!invoke || !desk_commands_enabled(commands,count,id))return 0;
    invoke(context,id);return 1;
}
int desk_command_menu(const struct desk_command *command,struct desk_menu_item *out) {
    if(!command || !out || !command->id || !command->label || strlen(command->label)>=sizeof(out->label) ||
       (command->shortcut && strlen(command->shortcut)>=sizeof(out->shortcut)) ||
       (command->flags&~(DESK_MENU_DISABLED|DESK_MENU_CHECKED)))return -1;
    struct desk_menu_item item={.id=command->id,.flags=command->flags};strcpy(item.label,command->label);
    if(command->shortcut)strcpy(item.shortcut,command->shortcut);
    *out=item;return 0;
}
