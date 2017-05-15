/*
 *  ReactOS kernel
 *  Copyright (C) 2004 ReactOS Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
/* COPYRIGHT:       See COPYING in the top level directory
 * PROJECT:         ReactOS text-mode setup
 * FILE:            base/setup/usetup/genlist.c
 * PURPOSE:         Generic list functions
 * PROGRAMMER:      Eric Kohl
 *                  Christoph von Wittich <christoph at reactos.org>
 */

/* INCLUDES *****************************************************************/

#include "usetup.h"

#define NDEBUG
#include <debug.h>

/* FUNCTIONS ****************************************************************/

typedef struct _GENERIC_LIST_ENTRY
{
    LIST_ENTRY Entry;
    PGENERIC_LIST List;
    PVOID UserData;
    CHAR Text[1];       // FIXME: UI stuff
} GENERIC_LIST_ENTRY;


typedef struct _GENERIC_LIST
{
    LIST_ENTRY ListHead;
    ULONG NumOfEntries;

    PGENERIC_LIST_ENTRY CurrentEntry;
    PGENERIC_LIST_ENTRY BackupEntry;
} GENERIC_LIST;


PGENERIC_LIST
CreateGenericList(VOID)
{
    PGENERIC_LIST List;

    List = (PGENERIC_LIST)RtlAllocateHeap(ProcessHeap,
                                          0,
                                          sizeof(GENERIC_LIST));
    if (List == NULL)
        return NULL;

    InitializeListHead(&List->ListHead);
    List->NumOfEntries = 0;

    List->CurrentEntry = NULL;
    List->BackupEntry = NULL;

    return List;
}

VOID
DestroyGenericList(
    IN OUT PGENERIC_LIST List,
    IN BOOLEAN FreeUserData)
{
    PGENERIC_LIST_ENTRY ListEntry;
    PLIST_ENTRY Entry;

    /* Release list entries */
    while (!IsListEmpty (&List->ListHead))
    {
        Entry = RemoveHeadList (&List->ListHead);
        ListEntry = CONTAINING_RECORD (Entry, GENERIC_LIST_ENTRY, Entry);

        /* Release user data */
        if (FreeUserData && ListEntry->UserData != NULL)
            RtlFreeHeap (ProcessHeap, 0, ListEntry->UserData);

        /* Release list entry */
        RtlFreeHeap (ProcessHeap, 0, ListEntry);
    }

    /* Release list head */
    RtlFreeHeap (ProcessHeap, 0, List);
}

BOOLEAN
AppendGenericListEntry(
    IN OUT PGENERIC_LIST List,
    IN PCHAR Text,
    IN PVOID UserData,
    IN BOOLEAN Current)
{
    PGENERIC_LIST_ENTRY Entry;

    Entry = (PGENERIC_LIST_ENTRY)RtlAllocateHeap(ProcessHeap,
                                                 0,
                                                 sizeof(GENERIC_LIST_ENTRY) + strlen(Text));
    if (Entry == NULL)
        return FALSE;

    strcpy (Entry->Text, Text);
    Entry->List = List;
    Entry->UserData = UserData;

    InsertTailList(&List->ListHead,
                   &Entry->Entry);
    List->NumOfEntries++;

    if (Current || List->CurrentEntry == NULL)
    {
        List->CurrentEntry = Entry;
    }

    return TRUE;
}


VOID
InitGenericListUi(
    IN OUT PGENERIC_LIST_UI ListUi,
    IN PGENERIC_LIST List)
{
    ListUi->List = List;
    ListUi->FirstShown = NULL;
    ListUi->LastShown = NULL;

    ListUi->Left = 0;
    ListUi->Top = 0;
    ListUi->Right = 0;
    ListUi->Bottom = 0;
    ListUi->Redraw = TRUE;
}

static
VOID
DrawListFrame(
    IN PGENERIC_LIST_UI ListUi)
{
    COORD coPos;
    DWORD Written;
    SHORT i;

    /* Draw upper left corner */
    coPos.X = ListUi->Left;
    coPos.Y = ListUi->Top;
    FillConsoleOutputCharacterA (StdOutput,
                                 0xDA, // '+',
                                 1,
                                 coPos,
                                 &Written);

    /* Draw upper edge */
    coPos.X = ListUi->Left + 1;
    coPos.Y = ListUi->Top;
    FillConsoleOutputCharacterA (StdOutput,
                                 0xC4, // '-',
                                 ListUi->Right - ListUi->Left - 1,
                                 coPos,
                                 &Written);

    /* Draw upper right corner */
    coPos.X = ListUi->Right;
    coPos.Y = ListUi->Top;
    FillConsoleOutputCharacterA (StdOutput,
                                 0xBF, // '+',
                                 1,
                                 coPos,
                                 &Written);

    /* Draw left and right edge */
    for (i = ListUi->Top + 1; i < ListUi->Bottom; i++)
    {
        coPos.X = ListUi->Left;
        coPos.Y = i;
        FillConsoleOutputCharacterA (StdOutput,
                                     0xB3, // '|',
                                     1,
                                     coPos,
                                     &Written);

        coPos.X = ListUi->Right;
        FillConsoleOutputCharacterA (StdOutput,
                                     0xB3, //'|',
                                     1,
                                     coPos,
                                     &Written);
    }

    /* Draw lower left corner */
    coPos.X = ListUi->Left;
    coPos.Y = ListUi->Bottom;
    FillConsoleOutputCharacterA (StdOutput,
                                 0xC0, // '+',
                                 1,
                                 coPos,
                                 &Written);

    /* Draw lower edge */
    coPos.X = ListUi->Left + 1;
    coPos.Y = ListUi->Bottom;
    FillConsoleOutputCharacterA (StdOutput,
                                 0xC4, // '-',
                                 ListUi->Right - ListUi->Left - 1,
                                 coPos,
                                 &Written);

    /* Draw lower right corner */
    coPos.X = ListUi->Right;
    coPos.Y = ListUi->Bottom;
    FillConsoleOutputCharacterA (StdOutput,
                                 0xD9, // '+',
                                 1,
                                 coPos,
                                 &Written);
}


static
VOID
DrawListEntries(
    IN PGENERIC_LIST_UI ListUi)
{
    PGENERIC_LIST List = ListUi->List;
    PGENERIC_LIST_ENTRY ListEntry;
    PLIST_ENTRY Entry;
    COORD coPos;
    DWORD Written;
    USHORT Width;

    coPos.X = ListUi->Left + 1;
    coPos.Y = ListUi->Top + 1;
    Width = ListUi->Right - ListUi->Left - 1;

    Entry = ListUi->FirstShown;
    while (Entry != &List->ListHead)
    {
        ListEntry = CONTAINING_RECORD (Entry, GENERIC_LIST_ENTRY, Entry);

        if (coPos.Y == ListUi->Bottom)
            break;
        ListUi->LastShown = Entry;

        FillConsoleOutputAttribute (StdOutput,
                                    (List->CurrentEntry == ListEntry) ?
                                    FOREGROUND_BLUE | BACKGROUND_WHITE :
                                    FOREGROUND_WHITE | BACKGROUND_BLUE,
                                    Width,
                                    coPos,
                                    &Written);

        FillConsoleOutputCharacterA (StdOutput,
                                     ' ',
                                     Width,
                                     coPos,
                                     &Written);

        coPos.X++;
        WriteConsoleOutputCharacterA (StdOutput,
                                      ListEntry->Text,
                                      min (strlen(ListEntry->Text), (SIZE_T)Width - 2),
                                      coPos,
                                      &Written);
        coPos.X--;

        coPos.Y++;
        Entry = Entry->Flink;
    }

    while (coPos.Y < ListUi->Bottom)
    {
        FillConsoleOutputAttribute (StdOutput,
                                    FOREGROUND_WHITE | BACKGROUND_BLUE,
                                    Width,
                                    coPos,
                                    &Written);

        FillConsoleOutputCharacterA (StdOutput,
                                     ' ',
                                     Width,
                                     coPos,
                                     &Written);
        coPos.Y++;
    }
}


static
VOID
DrawScrollBarGenericList(
    IN PGENERIC_LIST_UI ListUi)
{
    PGENERIC_LIST List = ListUi->List;
    COORD coPos;
    DWORD Written;

    coPos.X = ListUi->Right + 1;
    coPos.Y = ListUi->Top;

    if (ListUi->FirstShown != List->ListHead.Flink)
    {
        FillConsoleOutputCharacterA (StdOutput,
                                     '\x18',
                                     1,
                                     coPos,
                                     &Written);
    }
    else
    {
        FillConsoleOutputCharacterA (StdOutput,
                                     ' ',
                                     1,
                                     coPos,
                                     &Written);
    }

    coPos.Y = ListUi->Bottom;
    if (ListUi->LastShown != List->ListHead.Blink)
    {
        FillConsoleOutputCharacterA (StdOutput,
                                     '\x19',
                                     1,
                                     coPos,
                                     &Written);
    }
    else
    {
        FillConsoleOutputCharacterA (StdOutput,
                                     ' ',
                                     1,
                                     coPos,
                                     &Written);
    }
}


static
VOID
CenterCurrentListItem(
    IN PGENERIC_LIST_UI ListUi)
{
    PGENERIC_LIST List = ListUi->List;
    PLIST_ENTRY Entry;
    ULONG MaxVisibleItems, ItemCount, i;

    if ((ListUi->Top == 0 && ListUi->Bottom == 0) ||
        IsListEmpty(&List->ListHead) ||
        List->CurrentEntry == NULL)
    {
        return;
    }

    MaxVisibleItems = (ULONG)(ListUi->Bottom - ListUi->Top - 1);

/*****************************************
    ItemCount = 0;
    Entry = List->ListHead.Flink;
    while (Entry != &List->ListHead)
    {
        ItemCount++;
        Entry = Entry->Flink;
    }
*****************************************/
    ItemCount = List->NumOfEntries; // GetNumberOfListEntries(List);

    if (ItemCount > MaxVisibleItems)
    {
        Entry = &List->CurrentEntry->Entry;
        for (i = 0; i < MaxVisibleItems / 2; i++)
        {
            if (Entry->Blink != &List->ListHead)
                Entry = Entry->Blink;
        }

        ListUi->FirstShown = Entry;

        for (i = 0; i < MaxVisibleItems; i++)
        {
            if (Entry->Flink != &List->ListHead)
                Entry = Entry->Flink;
        }

        ListUi->LastShown = Entry;
    }
}


VOID
DrawGenericList(
    IN PGENERIC_LIST_UI ListUi,
    IN SHORT Left,
    IN SHORT Top,
    IN SHORT Right,
    IN SHORT Bottom)
{
    PGENERIC_LIST List = ListUi->List;

    ListUi->FirstShown = List->ListHead.Flink;
    ListUi->Left = Left;
    ListUi->Top = Top;
    ListUi->Right = Right;
    ListUi->Bottom = Bottom;

    DrawListFrame(ListUi);

    if (IsListEmpty(&List->ListHead))
        return;

    CenterCurrentListItem(ListUi);

    DrawListEntries(ListUi);
    DrawScrollBarGenericList(ListUi);
}


VOID
ScrollPageDownGenericList(
    IN PGENERIC_LIST_UI ListUi)
{
    SHORT i;

    /* Suspend auto-redraw */
    ListUi->Redraw = FALSE;

    for (i = ListUi->Top + 1; i < ListUi->Bottom - 1; i++)
    {
        ScrollDownGenericList(ListUi);
    }

    /* Update user interface */
    DrawListEntries(ListUi);
    DrawScrollBarGenericList(ListUi);

    /* Re enable auto-redraw */
    ListUi->Redraw = TRUE;
}


VOID
ScrollPageUpGenericList(
    IN PGENERIC_LIST_UI ListUi)
{
    SHORT i;

    /* Suspend auto-redraw */
    ListUi->Redraw = FALSE;

    for (i = ListUi->Bottom - 1; i > ListUi->Top + 1; i--)
    {
         ScrollUpGenericList(ListUi);
    }

    /* Update user interface */
    DrawListEntries(ListUi);
    DrawScrollBarGenericList(ListUi);

    /* Re enable auto-redraw */
    ListUi->Redraw = TRUE;
}


VOID
ScrollDownGenericList(
    IN PGENERIC_LIST_UI ListUi)
{
    PGENERIC_LIST List = ListUi->List;
    PLIST_ENTRY Entry;

    if (List->CurrentEntry == NULL)
        return;

    if (List->CurrentEntry->Entry.Flink != &List->ListHead)
    {
        Entry = List->CurrentEntry->Entry.Flink;
        if (ListUi->LastShown == &List->CurrentEntry->Entry)
        {
            ListUi->FirstShown = ListUi->FirstShown->Flink;
            ListUi->LastShown = ListUi->LastShown->Flink;
        }
        List->CurrentEntry = CONTAINING_RECORD(Entry, GENERIC_LIST_ENTRY, Entry);

        if (ListUi->Redraw)
        {
            DrawListEntries(ListUi);
            DrawScrollBarGenericList(ListUi);
        }
    }
}


VOID
ScrollToPositionGenericList(
    IN PGENERIC_LIST_UI ListUi,
    IN ULONG uIndex)
{
    PGENERIC_LIST List = ListUi->List;
    PLIST_ENTRY Entry;
    ULONG uCount = 0;

    if (List->CurrentEntry == NULL || uIndex == 0)
        return;

    do
    {
        if (List->CurrentEntry->Entry.Flink != &List->ListHead)
        {
            Entry = List->CurrentEntry->Entry.Flink;
            if (ListUi->LastShown == &List->CurrentEntry->Entry)
            {
                ListUi->FirstShown = ListUi->FirstShown->Flink;
                ListUi->LastShown = ListUi->LastShown->Flink;
            }
            List->CurrentEntry = CONTAINING_RECORD(Entry, GENERIC_LIST_ENTRY, Entry);
        }
        uCount++;
    }
    while (uIndex != uCount);

    if (ListUi->Redraw)
    {
        DrawListEntries(ListUi);
        DrawScrollBarGenericList(ListUi);
    }
}


VOID
ScrollUpGenericList(
    IN PGENERIC_LIST_UI ListUi)
{
    PGENERIC_LIST List = ListUi->List;
    PLIST_ENTRY Entry;

    if (List->CurrentEntry == NULL)
        return;

    if (List->CurrentEntry->Entry.Blink != &List->ListHead)
    {
        Entry = List->CurrentEntry->Entry.Blink;
        if (ListUi->FirstShown == &List->CurrentEntry->Entry)
        {
            ListUi->FirstShown = ListUi->FirstShown->Blink;
            ListUi->LastShown = ListUi->LastShown->Blink;
        }
        List->CurrentEntry = CONTAINING_RECORD(Entry, GENERIC_LIST_ENTRY, Entry);

        if (ListUi->Redraw)
        {
            DrawListEntries(ListUi);
            DrawScrollBarGenericList(ListUi);
        }
    }
}


VOID
RedrawGenericList(
    IN PGENERIC_LIST_UI ListUi)
{
    if (ListUi->List->CurrentEntry == NULL)
        return;

    if (ListUi->Redraw)
    {
        DrawListEntries(ListUi);
        DrawScrollBarGenericList(ListUi);
    }
}



VOID
SetCurrentListEntry(
    IN PGENERIC_LIST List,
    IN PGENERIC_LIST_ENTRY Entry)
{
    if (Entry->List != List)
        return;
    List->CurrentEntry = Entry;
}


PGENERIC_LIST_ENTRY
GetCurrentListEntry(
    IN PGENERIC_LIST List)
{
    return List->CurrentEntry;
}


PGENERIC_LIST_ENTRY
GetFirstListEntry(
    IN PGENERIC_LIST List)
{
    PLIST_ENTRY Entry = List->ListHead.Flink;

    if (Entry == &List->ListHead)
        return NULL;
    return CONTAINING_RECORD(Entry, GENERIC_LIST_ENTRY, Entry);
}


PGENERIC_LIST_ENTRY
GetNextListEntry(
    IN PGENERIC_LIST_ENTRY Entry)
{
    PLIST_ENTRY Next = Entry->Entry.Flink;

    if (Next == &Entry->List->ListHead)
        return NULL;
    return CONTAINING_RECORD(Next, GENERIC_LIST_ENTRY, Entry);
}


PVOID
GetListEntryUserData(
    IN PGENERIC_LIST_ENTRY Entry)
{
    return Entry->UserData;
}


LPCSTR
GetListEntryText(
    IN PGENERIC_LIST_ENTRY Entry)
{
    return Entry->Text;
}


ULONG
GetNumberOfListEntries(
    IN PGENERIC_LIST List)
{
    return List->NumOfEntries;
}



VOID
GenericListKeyPress(
    IN PGENERIC_LIST_UI ListUi,
    IN CHAR AsciiChar)
{
    PGENERIC_LIST List = ListUi->List;
    PGENERIC_LIST_ENTRY ListEntry;
    PGENERIC_LIST_ENTRY OldListEntry;
    BOOLEAN Flag = FALSE;

    ListEntry = List->CurrentEntry;
    OldListEntry = List->CurrentEntry;

    ListUi->Redraw = FALSE;

    if ((strlen(ListEntry->Text) > 0) && (tolower(ListEntry->Text[0]) == AsciiChar) &&
         (List->CurrentEntry->Entry.Flink != &List->ListHead))
    {
        ScrollDownGenericList(ListUi);
        ListEntry = List->CurrentEntry;

        if ((strlen(ListEntry->Text) > 0) && (tolower(ListEntry->Text[0]) == AsciiChar))
            goto End;
    }

    while (List->CurrentEntry->Entry.Blink != &List->ListHead)
        ScrollUpGenericList(ListUi);

    ListEntry = List->CurrentEntry;

    for (;;)
    {
        if ((strlen(ListEntry->Text) > 0) && (tolower(ListEntry->Text[0]) == AsciiChar))
        {
            Flag = TRUE;
            break;
        }

        if (List->CurrentEntry->Entry.Flink == &List->ListHead)
            break;

        ScrollDownGenericList(ListUi);
        ListEntry = List->CurrentEntry;
    }

    if (!Flag)
    {
        while (List->CurrentEntry->Entry.Blink != &List->ListHead)
        {
            if (List->CurrentEntry != OldListEntry)
                ScrollUpGenericList(ListUi);
            else
                break;
        }
    }

End:
    DrawListEntries(ListUi);
    DrawScrollBarGenericList(ListUi);

    ListUi->Redraw = TRUE;
}


VOID
SaveGenericListState(
    IN PGENERIC_LIST List)
{
    List->BackupEntry = List->CurrentEntry;
}


VOID
RestoreGenericListState(
    IN PGENERIC_LIST List)
{
    List->CurrentEntry = List->BackupEntry;
}


BOOLEAN
GenericListHasSingleEntry(
    IN PGENERIC_LIST List)
{
    if (!IsListEmpty(&List->ListHead) && List->ListHead.Flink == List->ListHead.Blink)
        return TRUE;

    /* if both list head pointers (which normally point to the first and last list member, respectively)
       point to the same entry then it means that there's just a single thing in there, otherwise... false! */
    return FALSE;
}

/* EOF */
