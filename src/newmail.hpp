#define MIN_MAIL_LEVEL        1
#define MAX_MAIL_SIZE         4096

#define MAIL_SQL_IDNUM        0
#define MAIL_SQL_SENDER_IDNUM 1
#define MAIL_SQL_SENDER_NAME  2
#define MAIL_SQL_RECIPIENT    3
#define MAIL_SQL_TIMESTAMP    4
#define MAIL_SQL_TEXT         5


void store_mail(long to, struct char_data *from, const char *message_pointer);
void raw_store_mail(long to, long from_id, const char *from_name, const char *message_pointer);

int amount_of_mail_waiting(struct char_data *ch);
char *get_and_delete_one_message(struct char_data *ch, char *sender);

SPECIAL(postmaster);

void postmaster_send_mail(struct char_data *ch, struct char_data *mailman, int cmd, char *arg);
void postmaster_check_mail(struct char_data *ch, struct char_data *mailman, int cmd, char *arg);
void postmaster_receive_mail(struct char_data *ch, struct char_data *mailman, int cmd, char *arg);
