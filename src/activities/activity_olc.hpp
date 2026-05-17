#ifndef __ACTIVITY_OLC_HPP__
#define __ACTIVITY_OLC_HPP__

#include "classes.hpp"
#include "../MenuFrameGenerics.hpp"

#define ACTIVITY_EDIT_activity_main 0
#define ACTIVITY_EDIT_activity_edit_slug 1
#define ACTIVITY_EDIT_activity_edit_display_name 2
#define ACTIVITY_EDIT_activity_edit_summary 3
#define ACTIVITY_EDIT_activity_edit_preconditions 4
#define ACTIVITY_EDIT_activity_edit_situations 5
#define ACTIVITY_EDIT_activity_edit_starting_situations 6
#define ACTIVITY_EDIT_check_main 7

#define CH (d->character)
#define ACT (d->edit_activity)
#define CHK (d->edit_check)
#define EFT (d->edit_effect)
#define OPT (d->edit_option)
#define OUT (d->edit_outcome)
#define SIT (d->edit_situation)
#define PARAMS (d->edit_params)

extern bool is_olc_available(struct char_data *ch);

extern void activity_activity_main_menu(struct descriptor_data *d);

MF_BOILERPLATE_CLASS(CheckMenuFrame);
MF_BOILERPLATE_CLASS(CheckFunctionMenuFrame);
MF_BOILERPLATE_CLASS(CheckParametersMenuFrame);

#endif // __ACTIVITY_OLC_HPP__