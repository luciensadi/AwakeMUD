#include "../src/structs.hpp"
#include "../src/dg_scripts.hpp"
#include <catch.hpp>
#include <cstring>

TEST_CASE("Updating a script context does not duplicate its variable", "[dg]") {
  trig_var_data *vars = NULL;
  add_var(&vars, "answer", "first", 1);
  add_var(&vars, "answer", "second", 2);
  add_var(&vars, "answer", "updated", 1);
  int count = 0;
  for (trig_var_data *v = vars; v; v = v->next) {
    count++;
    if (v->context == 1)
      REQUIRE(std::strcmp(v->value, "updated") == 0);
  }
  REQUIRE(count == 2);
  free_varlist(vars);
}

TEST_CASE("Script first-word extraction respects its destination", "[dg]") {
  trig_var_data value;
  value.value = str_dup("abcdefgh");
  char result[4];
  REQUIRE(text_processed("car", "", &value, result, sizeof(result)));
  REQUIRE(std::strcmp(result, "abc") == 0);
  delete [] value.value;
}

TEST_CASE("Script subfields belong to one replacement", "[dg]") {
  script_data sc;
  trig_data trig;
  add_var(&trig.var_list, "text", "abcdef", 0);
  char line[] = "%text.charat(1)% %text.charat(2)% %text.strlen%";
  char result[MAX_INPUT_LENGTH];
  var_subst(NULL, &sc, &trig, WLD_TRIGGER, line, result);
  REQUIRE(std::strcmp(result, "a b 6") == 0);
  free_varlist(trig.var_list);
}

TEST_CASE("Shadowrun field aliases reach their implementations", "[dg]") {
  char_data mob;
  script_data sc;
  trig_data trig;
  GET_NUYEN_RAW(&mob) = 10;
  GET_KARMA(&mob) = 20;
  GET_MAX_PHYSICAL(&mob) = 1000;
  GET_PHYSICAL(&mob) = 500;
  GET_BALLISTIC(&mob) = 4;
  char line[] = "%self.nuyen(5)% %self.karma(2)% %self.physical(-1)% %self.ballistic%";
  char result[MAX_INPUT_LENGTH];
  var_subst(&mob, &sc, &trig, MOB_TRIGGER, line, result);
  REQUIRE(std::strcmp(result, "15 22 4 4") == 0);
  REQUIRE(GET_NUYEN(&mob) == 15);
  REQUIRE(GET_KARMA(&mob) == 22);
  REQUIRE(GET_PHYSICAL(&mob) == 400);
}

TEST_CASE("Script message names include UID prefixes", "[dg]") {
  char input[] = "}10000001 waves.";
  char name[MAX_INPUT_LENGTH];
  char *rest = any_one_name(input, name);
  REQUIRE(std::strcmp(name, "}10000001") == 0);
  REQUIRE(std::strcmp(rest, " waves.") == 0);
}

TEST_CASE("Mob global fields can look up a plain variable", "[dg]") {
  char_data mob;
  script_data sc;
  trig_data trig;
  MOB_FLAGS(&mob).SetBit(MOB_ISNPC);
  mob.script = &sc;
  add_var(&sc.global_vars, "answer", "42", 0);
  char line[] = "%self.global(answer)%";
  char result[MAX_INPUT_LENGTH];
  var_subst(&mob, &sc, &trig, MOB_TRIGGER, line, result);
  REQUIRE(std::strcmp(result, "42") == 0);
  free_varlist(sc.global_vars);
}

TEST_CASE("Script wear flags use Awake wear locations", "[dg]") {
  obj_data obj;
  script_data sc;
  trig_data trig;
  obj.obj_flags.wear_flags.SetBit(ITEM_WEAR_WIELD);
  char line[MAX_INPUT_LENGTH], result[MAX_INPUT_LENGTH];
  snprintf(line, sizeof(line), "%%self.wearflag(%d)%%", WEAR_WIELD);
  var_subst(&obj, &sc, &trig, OBJ_TRIGGER, line, result);
  REQUIRE(std::strcmp(result, "1") == 0);
}

TEST_CASE("Detached scripts outlive their active callers", "[dg]") {
  room_data room;
  script_data *sc = room.script = new script_data;
  trig_data *trig = new trig_data;
  // No prototype is needed for this transient trigger.
  trig->nr = -1;
  add_trigger(sc, trig, -1);
  extract_script(&room, WLD_TRIGGER);
  REQUIRE(room.script == NULL);
  REQUIRE(trig->purged);
  REQUIRE(sc->purged);
  REQUIRE(TRIGGERS(sc) == NULL);
  dg_flush_purged_scripts();
}
