#ifndef __MENU_H__
#define __MENU_H__

#include "stdint.h"

#define MAX_MENU_SIZE 10
#define MAX_MENU_NAME_SIZE 20

typedef struct Menu
{
    char name[MAX_MENU_NAME_SIZE + 1];
    struct Menu *parent;
    struct Menu *children[MAX_MENU_SIZE];
    uint8_t childrenCount;
    void (*action)(void); // optionnel (si action directe)
} Menu_t;

typedef void (*MenuAction_t)(void);

void MENU_Create(Menu_t *menu, const char *name);
void MENU_AddSubmenu(Menu_t *menu, Menu_t *submenu);
void MENU_AddAction(Menu_t *menu, MenuAction_t action);

#endif