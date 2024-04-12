#include "bitfield.hpp"

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
#define SHOP_YES_GHOUL  5
#define SHOP_FLAGS  6

#define SHOP_LAST_IDNUM_LIST_SIZE 20

struct shop_data
{
  vnum_t vnum;
  vnum_t keeper;
  float profit_buy, profit_sell;
  sh_int random_amount, random_current, open, close, type;
  char *no_such_itemk, *no_such_itemp, *not_enough_nuyen, *doesnt_buy, *buy, *sell, *shopname;
  Bitfield buytypes, races, flags;
  int etiquette;
  struct shop_sell_data *selling;
  struct shop_order_data *order;

  shop_data() :
    vnum(NOTHING), keeper(NOBODY), profit_buy(0), profit_sell(0), random_amount(0),
    random_current(0), open(0), close(0), type(0), no_such_itemk(NULL),
    no_such_itemp(NULL), not_enough_nuyen(NULL), doesnt_buy(NULL), buy(NULL),
    sell(NULL), shopname(NULL), etiquette(0), selling(NULL), order(NULL)
  {}
};

struct shop_sell_data {
  vnum_t vnum;
  int type;
  int stock;
  int lastidnum[SHOP_LAST_IDNUM_LIST_SIZE];
  struct shop_sell_data *next;

  shop_sell_data() :
    next(NULL)
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
    item(NOTHING), player(NOBODY), timeavail(0), number(0), price(0),
    sent(0), paid(0), expiration(0), next(NULL)
  {}
};

extern const char *shop_flags[];
extern const char *shop_type[3];
extern const char *selling_type[];

int get_eti_test_results(struct char_data *ch, int eti_skill, int availtn, int availoff, int kinesics, int meta_penalty, int lifestyle, int pheromone_dice, int skill_dice);