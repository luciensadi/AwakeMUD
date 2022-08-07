#ifndef __playerdoc_h__
#define __playerdoc_h__

int alert_player_doctors_of_mort(struct char_data *ch, struct obj_data *docwagon);

void alert_player_doctors_of_contract_withdrawal(struct char_data *ch, bool withdrawn_because_of_death);

#endif
