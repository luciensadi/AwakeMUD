#define MAIL_COST 50

struct mail_data {
  long sender;
  long receiver;
  time_t timesent;
  char message[4096];
  struct mail_data *next;
  
  mail_data():
    sender(0), receiver(0), timesent(0), next(NULL)
  {}
};

