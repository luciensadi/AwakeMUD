#include "classes.hpp"

/*
Activity: A digraph containing one or more Situations as well as metadata about the activity itself. Data includes:
- authorship information (who, when, etc)
- identifying information (slug, display name)
- preconditions / requirements for it to be encountered
- dict of all possible situations in the activity, keyed by situation slug
- list of potential starting situation slugs to put the players in
*/