Inspired by Tyr's Ravencroft and Celestra games, this defines a choose-your-own-adventure type of system that lets us run characters through branching storylines.

They are invoked in the following situations:
- When traveling between disconnected destinations with the TRAVEL command (not yet implemented)
- (stretch) As opt-in experiences kicked off by players, possibly only in NERPcorp areas
- (stretch) As quests / autoruns

Characters enter Activities, which contain a list of Situations that are resolved by choosing one or more Options.
Based on the result of any checks/tests tied to that option, the characters reach an Outcome, which can route to another situation or end the Activity.
Outcomes and Situations both have optional Effects such as gaining/losing nuyen, taking damage, etc.
Preconditions are Checks 

Activity: An acyclic digraph containing one or more Situations as well as metadata about the Activity itself. Data includes:
- authorship information (who, when, etc)
- identifying information (slug (unique within the game), display name, description)
- [stretch] the flags for the Activity, a dict with:
  - pvp_mode: "cooperative" vs "competitive" (cooperative is default, competitive is stretch goal; if competitive is set, a score winner will be declared at end)
  - participation_mode: "individual" vs "group" (group is default, individual is stretch: limits number of players. Enables strict 1v1 competitions like arm wrestling)
  - team_mode: "off", "standard", "asymmetrical" (off is default, std/asym is stretch; if asym chosen, options must be allocated between teams as they have different goals)
  - elimination_mode: "off", "on" (off is default, on is stretch; if on, lowest scoring each round is removed from Activity and/or sent to hospital)
- a list of Checks representing preconditions / requirements for it to be encountered
- dict of all possible situations in the Activity, keyed by situation slug
- list of potential starting situation slugs to put the players in

Situation: A node in the Activity digraph containing one or more Options as well as metadata about the situation itself. Data includes:
- a list of Checks representing preconditions / requirements for it to be encountered (only applies when being evaluated as a starting situation)
- slug (unique within the activity)
- text to display to players when they enter the situation
- a list of Options (if list is empty, this is a terminal node; display the text and exit Activity)
- an optional list of Effects
- [stretch] an optional timeout dict that contains information on how long the players have to respond (opt) and what the timeout state routes to (opt).
  Non-specification results in a default timeout and a random option selection.

Option: A pair of Outcomes that are selected between based on the result of zero or more tests (default success). Data includes:
- a list of Checks representing preconditions / requirements for this option to be available to a given PC to select
- text to display to players in the options menu
- a list of checks or tests to run when the option is selected
- [stretch] an optional team list to assign this to teams in asymmetrical activities
- a "pass" dict describing the 'good' outcome (slug, effects applied from passing, etc)
- an optional "fail" dict describing the 'bad' outcome (slug, effects applied, etc); if missing in branching, failure ends the Activity; is ignored in linear Activities

Outcome: An edge in the digraph routing from a situation to either another situation or the end of the Activity. Data includes:
- the message displayed when this outcome is achieved, in the form of a sentence fragment that can follow "Assisted by X and Y, Lucien "
- an optional list of effects
- a list of Situation slugs that this outcome directs to (one will be randomly selected; empty if end of Activity)

Effect: A side effect that, when encountered, modifies the party in some way. Data includes:
- the name of the effect, which is portable across games ("apply_damage", "lose_money", "set_activity_flag", "quest", etc)
- a parameters dict that contains further information (e.g. "lose_money"'s parameters might be {"amount": 50}, "apply_damage" might be {"type":"physical", "amount":"100%"})
- a conditions dict that for MVP just has {"percent_chance": <int 1-100> }

Check: A test that outputs a score that's comparable across all checks. Data includes:
- the name of the effect, which is portable across games ("test_skill", "has_skill", "quest", "has_activity_flag", etc)
- a parameters dict that contains further information (e.g. "quest" might be {"type": "on_quest", "vnum": 3})
- a min-score field, defaulting to some reasonable value for a moderate-difficulty check, where if they score this or higher they pass this check - abstract away to low/moderate/high difficulty rating for creatives

When entering an Activity, the game randomly selects a viable starting Situation based on Preconditions (at least one PC must match to see it). Then:
1. Show the Situation's intro text to the players.
2. Apply any Effects from entering the situation. If anyone goes down, stop the whole thing and evac the party to the starting area.
3. For all PCs, list all Options that that specific PC can do, based on Preconditions
4. All players select an Option; if a PC does not select an option within in X seconds, game randomly selects one for them
5. Game randomly picks one player's Option selection and runs the checks/tests for every PC who selected that option; a PC must achieve all tests to pass
6. If any PC passed, output the "pass" Outcome's text, led by the PC that rolled highest; if all failed, output the "fail" outcome's text.
7. Apply any Effects from the selected outcome.
8. Advance the Activity to the Situation described by the Outcome (end Activity if no situation is listed), then start over from 1.


Additionally (not represented in the data structures above), each character has an 'effort' pool for each new Activity that fills to some amount at start
and then increases by a set percentage with each new situation. When picking an option, you can specify an effort level from low/mod/high, which drains an
amount from the pool and modifies your result accordingly (raising/lowering TN, etc). This allows you to choose what you really want to put in effort on /
where you want your character to shine most.


Example Activity flow:

<desc for starting situation> While on a shortcut through a blind alley, you Activity a sneering thug. "Your money or your life, kid," he sneers sneeringly.

Options:
1) Kick him in the nads and run (Brawling and Athletics, moderately difficult)
2) Pay him 1000 nuyen (50% chance of success)
3) Pay him 10000 nuyen (100% chance of success)
4) Fight him (enters combat)

> 1

<output for successful test on option 1> Aided by Vile and Jank, Lucien raises his hands in a placating manner, then buries the toe of his boot in the middle of the thug's family jewels when he relaxes his guard.

<desc for next situation, which is a combat one> With the thug on the ground, you hoof it for the exit, only to be intercepted by a pack of goons with overly-polished SMGs at the ready. This time, they don't give you a chance to bluff.
<enter combat, spawning two goons per player in party>
<combat completes successfully, auto-following to next situation>

<desc for ending situation> You leap over their broken bodies, making a hasty retreat. Cries of outrage ring out from behind you...
<end of Activity>