#ifndef _chargen_h
#define  _chargen_h

/* chargen connected modes */
#define CCR_AWAIT_CR                 -1
#define CCR_PRONOUNS                 0
#define CCR_RACE                     1
#define CCR_TOTEM                    2
#define CCR_PRIORITY                 3
#define CCR_ASSIGN                   4
#define CCR_TRADITION                5
#define CCR_ASPECT                   6
#define CCR_TOTEM2                   7
#define CCR_TYPE                     8
#define CCR_POINTS                   9
#define CCR_PO_ATTR                  10
#define CCR_PO_SKILL                 11
#define CCR_PO_RESOURCES             12
#define CCR_PO_MAGIC                 13
#define CCR_ARCHETYPE_MODE           14
#define CCR_ARCHETYPE_SELECTION_MODE 15
#define CCR_MAGE                     16

#define PO_RACE		0
#define PO_ATTR		1
#define PO_SKILL	2
#define PO_RESOURCES	3
#define PO_MAGIC	4

/* priority choosing chargen modes */
#define PR_NONE         0
#define PR_ATTRIB       1
#define PR_MAGIC        2
#define PR_RESOURCE     3
#define PR_SKILL        4
#define PR_RACE         5

#define NUM_CCR_PR_POINTS 6

extern const char *pc_race_types[];
#endif
