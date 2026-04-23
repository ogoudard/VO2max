/************************************
 * INCLUDES
 ************************************/

#include "menu.h"
#include "string.h"

/************************************
 * PRIVATE MACROS AND DEFINES
 ************************************/

/************************************
 * PRIVATE VARIABLES
 ************************************/

/************************************
 * PUBLIC FUNCTION DEFINITIONS
 ************************************/

void MENU_Create(Menu_t *menu, const char *name)
{
    menu->action = NULL;
    menu->parent = NULL;
    menu->childrenCount = 0;
    strncpy(menu->name, name, MAX_MENU_NAME_SIZE);
}

void MENU_AddSubmenu(Menu_t *menu, Menu_t *submenu)
{
    if(menu->childrenCount < (MAX_MENU_SIZE - 1))
    {
        menu->children[menu->childrenCount] = submenu;
        menu->children[menu->childrenCount]->parent = menu;
        menu->childrenCount++;
    }
}

void MENU_AddAction(Menu_t *menu, MenuAction_t action)
{
    menu->action = action;
}