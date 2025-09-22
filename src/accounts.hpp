#ifndef __ACCOUNTS_HPP__
#define __ACCOUNTS_HPP__

class Account {
  idnum_t idnum;
  const char *email;
  int syspoints;
public:
  // Create a new account. You must call save() to initialize it in the DB.
  Account();

  // Load an existing account based on idnum.
  Account(idnum_t idnum);

  void save();
};

#endif //__ACCOUNTS_HPP__