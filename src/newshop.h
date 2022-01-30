#include "bitfield.h"

#define SELL_ALWAYS  0
#define SELL_AVAIL  1
#define SELL_STOCK   2
#define SELL_BOUGHT  3

#define SHOP_GREY  0
#define SHOP_LEGAL  1
#define SHOP_BLACK  2

#define SHOP_DOCTOR  1
#define SHOP_WONT_NEGO  2
#define SHOP_NORESELL  3
#define SHOP_CHARGEN  4
#define SHOP_FLAGS  5

#define SHOP_LAST_IDNUM_LIST_SIZE 20

struct shop_data
{
  vnum_t vnum;
  vnum_t keeper;
  float profit_buy, profit_sell;
  sh_int random_amount, random_current, open, close, type;
  char *no_such_itemk, *no_such_itemp, *not_enough_nuyen, *doesnt_buy, *buy, *sell, *shopname;
  Bitfield buytypes, races, flags;
  int ettiquete;
  struct shop_sell_data *selling;
  struct shop_order_data *order;
  shop_data() :
    no_such_itemk(NULL), no_such_itemp(NULL), not_enough_nuyen(NULL), doesnt_buy(NULL), buy(NULL), sell(NULL),
    shopname(NULL), selling(NULL), order(NULL)
  {}
};

struct shop_sell_data {
  vnum_t vnum;
  int type;
  int stock;
  int lastidnum[SHOP_LAST_IDNUM_LIST_SIZE];
  struct shop_sell_data *next;
  
  shop_sell_data() :
    vnum(0), type(0), stock(0), next(NULL)
  {
    memset(lastidnum, 0, SHOP_LAST_IDNUM_LIST_SIZE * sizeof(int));
  }
};

struct shop_order_data {
  vnum_t item;
  vnum_t player;
  int timeavail;
  int number;
  int price;
  bool sent;
  int paid;
  long expiration;
  struct shop_order_data *next;

  shop_order_data() :
    item(0), player(0), timeavail(0), number(0), price(0), sent(0), paid(0), expiration(0), next(NULL)
  {}
};

extern const char *shop_flags[];
extern const char *shop_type[3];
extern const char *selling_type[];
