#include <vector>

#include "utils.hpp"
#include "interpreter.hpp"
#include "newdb.hpp"

/* The policy is a tree. The root document contains all superheadings, which contain their own headings, etc. */

class PolicyNode {
public:
  const char *title   = NULL;
  const char *summary = NULL;
  // time_t last_updated;

  std::vector<const char *> children = {};

  PolicyNode() {};

  PolicyNode(const char *title, const char *summary) :
    title(str_dup(title)), summary(str_dup(summary))
  {}

  ~PolicyNode() {
    delete[] title;
    delete[] summary;
    children.clear();
  }

  void add_child(const char *child) { children.push_back(str_dup(child)); }
};

std::vector<PolicyNode *> root_policy_document = {};

void cleanup_policy_tree() {
  root_policy_document.clear();
}

void initialize_policy_tree() {
  // Policy 0: Don't be a dick.
  {
    PolicyNode *section_root = new PolicyNode(
      "Play Nice",
      "Respect your fellow players, staff, and the game."
    );
    root_policy_document.push_back(section_root);

    section_root->add_child("'Play Nice' is the catch-all rule for when other rules don't cover the scenario. We do our best to document the policies and rules of the game in this POLICY command, but for the edge cases and odd scenarios, this rule is the one we'll fall back on. To avoid being caught up in this rule, be respectful towards others and play well. We're all here to have fun and unwind, and as long as you're playing in that spirit and not harming anyone in the process, you'll be A-OK.");
    section_root->add_child("Some specific things that you should respect are: communication preferences, consent, pronouns, opt-outs of various interactions, etc.");
  }

  // Staff authority.
  {
    char escalation_string[2048];

    PolicyNode *section_root = new PolicyNode(
      "Staff Authority",
      "It's absolute in game, extremely limited outside of it."
    );
    root_policy_document.push_back(section_root);

    const char *game_owner_name = get_player_name(1);
    snprintf(escalation_string, sizeof(escalation_string), 
             "Staff are volunteers who are giving their time to help the game run more smoothly. If a staff member tells you to do something, please respect it. If you believe they're abusing their power, raise it with %s (%s).",
             game_owner_name,
             STAFF_CONTACT_EMAIL);
    delete [] game_owner_name;

    section_root->add_child(escalation_string);
    section_root->add_child("Staff are not expected to adjudicate things that happened outside of the MUD. We ^YHIGHLY^n recommend keeping all communication with other players to the MUD or the official Discord server.");
    section_root->add_child("Joining other servers, chatting on /r/MUD, or otherwise interacting outside of the game are all allowed, but keep in mind that staff don't have visibility into those environments. You participate in unofficial communities at your own risk.");
  }

  // Multiplaying.
  {
    PolicyNode *section_root = new PolicyNode(
      "Limited Multiplaying",
      "You can only have one active character, and your chars cannot benefit each other."
    );
    root_policy_document.push_back(section_root);

    section_root->add_child("You may have multiple characters online simultaneously, but only one can be active at once-- the others must be idling or engaged in build/repair tasks. Brief stints of dual activity are fine in limited cases (ex: running to buy something, helping out another player, bantering on the radio, etc).");
    section_root->add_child("Your characters may never benefit each other in any way. This includes, but is not limited to, transferring items or nuyen between them or having them share vehicles or apartments. This also includes dropping gear, deleting your char, recreating, and getting that gear again. When in doubt, ask staff!");
    section_root->add_child("You may not have multiple characters in the same player group (pgroup). You may also not have characters split across opposing or conflicting groups.");
    section_root->add_child("Wherever possible, your alts should not acknowledge each other's existence. Be noncommittal when alt A is asked about alt B, don't have A and B talk to each other on the radio, etc.");
  }

  // Underage characters.
  {
    PolicyNode *section_root = new PolicyNode(
      "No Underage Characters",
      "All CHARACTERS must be aged 20 or older, and must present as such."
    );
    root_policy_document.push_back(section_root);

    section_root->add_child("Because we don't forbid adult roleplay here, we require that all characters are 20+, and that they present as such.");
    section_root->add_child("It is a violation of this policy to have a character who IS, APPEARS, or IDENTIFIES AS underage. Such characters will be banned.");
    section_root->add_child("Additionally, anyone who engages in adult scenes with a character in violation of this policy will also be banned.");
  }

  // Underage players.
  {
    PolicyNode *section_root = new PolicyNode(
      "No Underage Players",
      "All PLAYERS must be aged 18 or older, and must present as such."
    );
    root_policy_document.push_back(section_root);

    section_root->add_child("For legal reasons, we require that all players are aged 18 or older.");
    section_root->add_child("We sympathize with folks looking for a MUD to play in before they reach 18 years of age, but for legal reasons we cannot accommodate them here. Our hands are tied, and we will remove anyone found to be underage.");
  }

  // Cheating.
  {
    PolicyNode *section_root = new PolicyNode(
      "No Cheating / Exploiting",
      "If it feels too good to be true, it probably is."
    );
    root_policy_document.push_back(section_root);

    section_root->add_child("This codebase has been under continual development since the early 1990s, and is guaranteed to have unintentional bugs and loopholes in it that can be used for personal gain. Using these bugs and loopholes, aka 'exploiting', is not allowed. Instead, report the bug/loophole/exploit, either via the ^WBUG^n command or through a support ticket in Discord, and you'll be rewarded.");
  }

  // Botting.
  {
    PolicyNode *section_root = new PolicyNode(
      "Limited Triggers, No Botting",
      "No automated play."
    );
    root_policy_document.push_back(section_root);

    section_root->add_child("Botting (aka setting up your client to play the game for you, whether or not you're at the keyboard) is not allowed.");
    section_root->add_child("Triggers are restricted. They can only be used to produce non-material outcomes that don't directly benefit or harm anyone. For example, an answering machine trigger that recites a canned message after a certain number of rings is allowed, but an auto-killing trigger is not.");
    section_root->add_child("Auto-looting is a specific type of trigger that you're allowed to write, but tune it so that you don't steal things from other players' kills-- if we get complaints about auto-looters, they will no longer be allowed.");
    section_root->add_child("Aspirin-eating triggers are also specifically allowed.");
  }

  // Restringing
  {
    PolicyNode *section_root = new PolicyNode(
      "Limitations on Restringing, Painting, Decorating, etc",
      "These must be similar to the base object."
    );
    root_policy_document.push_back(section_root);

    section_root->add_child("We give players wide latitude in restringing / customizing the cosmetic aspects of their gear, but we ask that you don't take this too far.");
    section_root->add_child("Restrings/customization must be similar to the base object (e.g. restringing 'an armored vest' into 'a black tactical vest' is fine, restringing it as a t-shirt is not.)");
    section_root->add_child("Decorations of apartments, vehicles, etc must be in line with the flavor and theme of the original description. Decorating a Squatter apartment with gold finishes and marble floors is an extreme example of what's not allowed.");
    section_root->add_child("Customizations in violation of this policy can result in loss of the customized object/vehicle/apartment/etc without refund.");
  }

  // Impersonation
  {
    PolicyNode *section_root = new PolicyNode(
      "Impersonation",
      "Don't forge others' identity."
    );
    root_policy_document.push_back(section_root);

    section_root->add_child("Using the ^WCUSTOMIZE^n command to impersonate someone else, posing as them, emoting with them taking actions, etc are all not allowed.");
    section_root->add_child("We want our players, staff, and NPCs to act under their own agency without being impersonated or convincingly mimicked.");
  }

  // Roleplaying
  {
    PolicyNode *section_root = new PolicyNode(
      "Roleplaying and Consent",
      "We are fully opt-in and consent-based."
    );
    root_policy_document.push_back(section_root);

    section_root->add_child("^cRoleplaying is encouraged, but not required.^n You may opt out of roleplaying if you so choose. However, even if you opt out, we ask that you refrain from discussing OOC concepts in IC mediums such as on the radio.");
    section_root->add_child("^cRoleplaying is consent-based.^n All players involved must consent to a scene. While assumed consent holds for ordinary scenes, anything with conflict, spying, adult situations, aggressive actions or speech, etc needs to be pre-approved by participants. ^WWe recommend using the TELL command to obtain consent so that it can be proven.^n");
    section_root->add_child("^cActive consent required.^n If at any time a player becomes uncomfortable and makes this known (via the ^WCONSENT^n command, osays, tells, or any other in-game medium), the scene must immediately be stopped. If you cannot reach a consensus with your roleplay partner(s) about the outcome of a scene, contact RP staff for assistance.");
    section_root->add_child("^cCertain topics are forbidden.^n Players may not enact scenes featuring suicide or sexual assault. GMs MAY request staff approval for topic inclusion in tabletop runs on a case-by-case basis, but will need to include content warnings if approved.");
    section_root->add_child("^cRoleplaying is supposed to be fun.^n If you're not having fun, step back and send a message (via osays or tells) to figure out what's going on and how you can steer the scene so you're all enjoying yourselves.");
    section_root->add_child("^cIRL racism/prejudices forbidden.^n Your character can have problems with metatypes (dwarves, elves, etc), but you may not roleplay prejudices that appear IRL, specifically including but not limited to racism, sexism, homophobia, etc. The use of slurs or other derogatory depictions of IRL peoples is explicitly forbidden.");
    section_root->add_child("^cPermadeath is strictly optional.^n You can choose to have your character's life permanently end at any point, but this can never be forced on you.");
    section_root->add_child("^cAdult content must be kept to private spaces.^n Aside from our restrictions on underage characters and the forbidden topics listed above, we don't police your roleplay. However, due to the impossibility of obtaining full consent from everyone who might drop in on a public adult scene, we require that any adult content (i.e. sexual in nature) is written exclusively in locked areas (apartments, vehicles, etc). Aim for a PG-13 rating for public content.");
  }

  // Staff Limitations
  {
    PolicyNode *section_root = new PolicyNode(
      "Rules for Staff",
      "With great power comes great responsibility."
    );
    root_policy_document.push_back(section_root);

    section_root->add_child("Staff may not interact with their non-staff characters in any way without prior consensus approval of other staff members (ex: for diagnosing issues, fixing bugs, etc). Any such interactions must be explained in the admin audit log.");
    section_root->add_child("Staff must always identify themselves as staff members before engaging in competitive, adult, or other situations that could allow abuse of position. Such identification must make it clear that there is absolutely no coercion and that the other player(s) are free to bow out with no risk of repercussion.");
    section_root->add_child("Staff may never use their position for personal gain.");
    section_root->add_child("Staff must be as impartial as possible and should not play favorites.");
    section_root->add_child("To enable us to play the game without receiving special treatment, outing staff alts is not allowed for anyone, staff or not. Staff are still required to self-disclose before participating in scenes where their position could invoke a power imbalance.");
  }

  // Topic Limitations
  {
    PolicyNode *section_root = new PolicyNode(
      "Limitations on Discussion Topics",
      "If you wouldn't say it at a big family gathering..."
    );
    root_policy_document.push_back(section_root);

    section_root->add_child("Most players hop on MUDs to relax and unwind away from the stressors of everyday life. To enable this, we ask that you not discuss certain topics on global channels or in Discord.");
    section_root->add_child("The restricted topics are: politics, religion, COVID/vaccination, and anything else you can reasonably expect strong disagreement on.");
  }

  // Copyright / Trademark Respect
  {
    PolicyNode *section_root = new PolicyNode(
      "Don't Share Copyrighted Works",
      "PDFs etc must be bought instead of shared."
    );
    root_policy_document.push_back(section_root);

    section_root->add_child("Although we are a 100% cash-free game and we make no claims to any trademarks or copyrights, we still exist at the mercy of the rightsholders. As such, we require that any used PDFs are purchased from authorized retailers (e.g. DriveThruRPG) instead of shared.");
    section_root->add_child("^yThis is a very strict rule.^n Please don't even hint about going around it or ask sly questions about Google search terms etc-- it's legitimately purchased copies or nothing.");
  }

#ifdef ENABLE_PK
  // Player Killing / CvC.
  {
    PolicyNode *section_root = new PolicyNode(
      "Character vs Character / Player Killing",
      "CvC is purely opt-in. If everyone involved is not enjoying themselves, pull back."
    );
    root_policy_document.push_back(section_root);

    section_root->add_child("CvC is allowed as long as all involved parties are flagged PK. Non-PK targets are strictly off-limits.");
    section_root->add_child("Forms of CvC include, but are not limited to: Attacking characters, casting hostile or harmful spells, damaging their vehicles, etc.");
    section_root->add_child("You must have a good in-character reason for fighting. 'My character is a hunter/psycho' is not a valid reason for CvC. This also means that you cannot take OOC grievances IC to attack their characters.");
    section_root->add_child("Don't harass or trick people into toggling PK. Again, it's expected to be a purely opt-in experience.");
    section_root->add_child("Griefing (repeatedly killing a target that is not attacking you) is not allowed. This explicitly includes, but isn't limited to, spawncamping them at the Docwagon areas.");
    section_root->add_child("PKing in a way that's explicitly intended to destroy gear is not allowed (e.g. ignite spam).");
  }

  // PK Required When
  {
    PolicyNode *section_root = new PolicyNode(
      "CvC/PK Opt-In Situationally Required",
      "If you're playing as an antagonist, PK is required."
    );
    root_policy_document.push_back(section_root);

    section_root->add_child("CvC is allowed as long as all involved parties are flagged PK. Non-PK targets are strictly off-limits.");
    section_root->add_child("Forms of CvC include, but are not limited to: Attacking characters, casting hostile or harmful spells, damaging their vehicles, etc.");
    section_root->add_child("You must have a good in-character reason for fighting. 'My character is a hunter/psycho' is not a valid reason for CvC. This also means that you cannot take OOC grievances IC to attack their characters.");
    section_root->add_child("Don't harass or trick people into toggling PK. Again, it's expected to be a purely opt-in experience.");
    section_root->add_child("Griefing (repeatedly killing a target that is not attacking you) is not allowed. This explicitly includes, but isn't limited to, spawncamping them at the Docwagon areas.");
    section_root->add_child("PKing in a way that's explicitly intended to destroy gear is not allowed (e.g. ignite spam).");
  }
#endif

  // Policy addendums
  {
    PolicyNode *section_root = new PolicyNode(
      "Policy Addendums",
      "They're scattered around. Sorry."
    );
    root_policy_document.push_back(section_root);

    section_root->add_child("Over time, we've accrued policy addendums in helpfiles etc, usually along the lines of 'if you use this feature in X way, it's an exploit'. We usually try to fix things instead of writing new policies to cover them, but that's not always feasible, hence those updates.");
    section_root->add_child("These addendums, which may show up in helpfiles or the ^WMOTD^n, are considered policy as well.");
    section_root->add_child("If you come across an addendum that's not reflected in this ^WPOLICY^n command, ^WIDEA^n its inclusion for a reward.");
  }

  {
    PolicyNode *section_root = new PolicyNode(
      "Policies are Guidelines",
      "We're not lawyers. Follow the spirit instead of the letter."
    );
    root_policy_document.push_back(section_root);

    section_root->add_child("Staff are a group of hobbyist volunteers who are doing our best to outline the rules and policies here. However, as we're humans, we'll make mistakes and omit things. Do your best to follow the spirit of the rules instead of just the letter of them and we shouldn't have any issues.");
    section_root->add_child("Because these policies are incomplete by nature, they're subject to change without notice. We'll do our best to let everyone know when they do change, though.");
    section_root->add_child("^WRemember: when in doubt, ask staff!^n");
  }
}

ACMD(do_policy) {
  int policy_idx = 0, selected_idx = 0;

  skip_spaces(&argument);

  // POLICY on its own gives you a summarized list of all the policies.
  if (!*argument) {
    send_to_char("The ^Wshort versions^n of the current policies are:\r\n", ch);
    for (auto &node : root_policy_document) {
      send_to_char(ch, "^W%3d)^c %s^n: %s\r\n", policy_idx++, node->title, node->summary);
    }
    send_to_char("\r\nType ^WPOLICY <number>^n for more details.\r\n", ch);
    return;
  }

  // POLICY <number> drills down.
  FAILURE_CASE(!isdigit(*argument), "Syntax: ^WPOLICY <number>^n, like ^WPOLICY 1^n.");
  FAILURE_CASE((policy_idx = selected_idx = atoi(argument)) < 0, "You must supply a positive number.");

  for (auto &node : root_policy_document) {
    if (selected_idx-- > 0)
      continue;

    FAILURE_CASE(node->children.empty(), 
                 "No further information is available on that policy. Reach out to staff if you need clarification.");
    
    send_to_char(ch, "^WPOLICY %d:^n ^c%s^n\r\n\r\n", policy_idx, node->title);
    
    int subheading = 1;
    for (auto content : node->children) {
      send_to_char(ch, "^W%3d.%d)^n %s^n\r\n\r\n", policy_idx, subheading++, content);
    }
    return;
  }

  send_to_char("That's not a valid policy number. Syntax: ^WPOLICY <number>^n, like ^WPOLICY 1^n.\r\n", ch);
}