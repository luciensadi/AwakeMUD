#include "awake.h"
#include "utils.h"
#include "lexicons.h"

int lexicon_sizes[NUM_LEXICONS];

int get_lexicon_number_for_language(int language_skill) {
  switch (language_skill) {
    case SKILL_ENGLISH    : return LEXICON_ENGLISH;
    case SKILL_SPERETHIEL : return LEXICON_SPERETHIEL;
    case SKILL_SPANISH    : return LEXICON_SPANISH;
    case SKILL_JAPANESE   : return LEXICON_JAPANESE;
    case SKILL_CHINESE    : return LEXICON_CHINESE;
    case SKILL_KOREAN     : return LEXICON_KOREAN;
    case SKILL_ITALIAN    : return LEXICON_ITALIAN;
    case SKILL_RUSSIAN    : return LEXICON_RUSSIAN;
    case SKILL_SIOUX      : return LEXICON_SIOUX;
    case SKILL_MAKAW      : return LEXICON_MAKAW;
    case SKILL_CROW       : return LEXICON_CROW;
    case SKILL_SALISH     : return LEXICON_SALISH;
    case SKILL_UTE        : return LEXICON_UTE;
    case SKILL_NAVAJO     : return LEXICON_NAVAJO;
    case SKILL_GERMAN     : return LEXICON_GERMAN;
    case SKILL_ORZET      : return LEXICON_ORZET;
    case SKILL_ARABIC     : return LEXICON_ARABIC;
    case SKILL_LATIN      : return LEXICON_LATIN;
    case SKILL_GAELIC     : return LEXICON_GAELIC;
    case SKILL_FRENCH     : return LEXICON_FRENCH;
    
    default: return LEXICON_ENGLISH;
  }
}

void populate_lexicon_size_table() {
  int lexicon_number, index;
  
  for (int i = 0; i < MAX_SKILLS; i++) {
    if (SKILL_IS_LANGUAGE(i) && (lexicon_number = get_lexicon_number_for_language(i)) >= 0) {
      for (index = 0; *(lexicons[lexicon_number][index]) != '\n'; index++);
      lexicon_sizes[lexicon_number] = index - 1;
    }
  }
}

const char *get_random_word_from_lexicon(int language_skill) {
  int lexicon_number = get_lexicon_number_for_language(language_skill);
  
  return lexicons[lexicon_number][number(0, lexicon_sizes[lexicon_number])];
}

const char **lexicons[] = {
  lexicon_english,
  lexicon_sperethiel,
  lexicon_spanish,
  lexicon_japanese,
  lexicon_chinese,
  lexicon_korean,
  lexicon_italian,
  lexicon_russian,
  lexicon_sioux,
  lexicon_makaw,
  lexicon_crow,
  lexicon_salish,
  lexicon_ute,
  lexicon_navajo,
  lexicon_german,
  lexicon_orzet,
  lexicon_arabic,
  lexicon_latin,
  lexicon_gaelic,
  lexicon_french
};

const char *lexicon_english[] = {
  "ability", "able", "about", "above", "accept", "according", "account", "across", "act", "action", "activity", "actually", "add", "address", "administration", "admit", "adult", "affect", "after", "again", "against", "age", "agency", "agent", "ago", "agree", "agreement", "ahead", "air", "all", "allow", "almost", "alone", "along", "already", "also", "although", "always", "American", "among", "amount", "analysis", "and", "animal", "another", "answer", "any", "anyone", "anything", "appear", "apply", "approach", "area", "argue", "arm", "around", "arrive", "art", "article", "artist", "as", "ask", "assume", "at", "attack", "attention", "attorney", "audience", "author", "authority", "available", "avoid", "away", "baby", "back", "bad", "bag", "ball", "bank", "bar", "base", "be", "beat", "beautiful", "because", "become", "bed", "before", "begin", "behavior", "behind", "believe", "benefit", "best", "better", "between", "beyond", "big", "bill", "billion", "bit", "black", "blood", "blue", "board", "body", "book", "born", "both", "box", "boy", "break", "bring", "brother", "budget", "build", "building", "business", "but", "buy", "by", "call", "camera", "campaign", "can", "cancer", "candidate", "capital", "car", "card", "care", "career", "carry", "case", "catch", "cause", "cell", "center", "central", "century", "certain", "certainly", "chair", "challenge", "chance", "change", "character", "charge", "check", "child", "choice", "choose", "church", "citizen", "city", "civil", "claim", "class", "clear", "clearly", "close", "coach", "cold", "collection", "college", "color", "come", "commercial", "common", "community", "company", "compare", "computer", "concern", "condition", "conference", "Congress", "consider", "consumer", "contain", "continue", "control", "cost", "could", "country", "couple", "course", "court", "cover", "create", "crime", "cultural", "culture", "cup", "current", "customer", "cut", "dark", "data", "daughter", "day", "dead", "deal", "death", "debate", "decade", "decide", "decision", "deep", "defense", "degree", "Democrat", "democratic", "describe", "design", "despite", "detail", "determine", "develop", "development", "die", "difference", "different", "difficult", "dinner", "direction", "director", "discover", "discuss", "discussion", "disease", "do", "doctor", "dog", "door", "down", "draw", "dream", "drive", "drop", "drug", "during", "each", "early", "east", "easy", "eat", "economic", "economy", "edge", "education", "effect", "effort", "eight", "either", "election", "else", "employee", "end", "energy", "enjoy", "enough", "enter", "entire", "environment", "environmental", "especially", "establish", "even", "evening", "event", "ever", "every", "everybody", "everyone", "everything", "evidence", "exactly", "example", "executive", "exist", "expect", "experience", "expert", "explain", "eye", "face", "fact", "factor", "fail", "fall", "family", "far", "fast", "father", "fear", "federal", "feel", "feeling", "few", "field", "fight", "figure", "fill", "film", "final", "finally", "financial", "find", "fine", "finger", "finish", "fire", "firm", "first", "fish", "five", "floor", "fly", "focus", "follow", "food", "foot", "for", "force", "foreign", "forget", "form", "former", "forward", "four", "free", "friend", "from", "front", "full", "fund", "future", "game", "garden", "gas", "general", "generation", "get", "girl", "give", "glass", "go", "goal", "good", "government", "great", "green", "ground", "group", "grow", "growth", "guess", "gun", "guy", "hair", "half", "hand", "hang", "happen", "happy", "hard", "have", "he", "head", "health", "hear", "heart", "heat", "heavy", "help", "her", "here", "herself", "high", "him", "himself", "his", "history", "hit", "hold", "home", "hope", "hospital", "hot", "hotel", "hour", "house", "how", "however", "huge", "human", "hundred", "husband", "I", "idea", "identify", "if", "image", "imagine", "impact", "important", "improve", "in", "include", "including", "increase", "indeed", "indicate", "individual", "industry", "information", "inside", "instead", "institution", "interest", "interesting", "international", "interview", "into", "investment", "involve", "issue", "it", "item", "its", "itself", "job", "join", "just", "keep", "key", "kid", "kill", "kind", "kitchen", "know", "knowledge", "land", "language", "large", "last", "late", "later", "laugh", "law", "lawyer", "lay", "lead", "leader", "learn", "least", "leave", "left", "leg", "legal", "less", "let", "letter", "level", "lie", "life", "light", "like", "likely", "line", "list", "listen", "little", "live", "local", "long", "look", "lose", "loss", "lot", "love", "low", "machine", "magazine", "main", "maintain", "major", "majority", "make", "man", "manage", "management", "manager", "many", "market", "marriage", "material", "matter", "may", "maybe", "me", "mean", "measure", "media", "medical", "meet", "meeting", "member", "memory", "mention", "message", "method", "middle", "might", "military", "million", "mind", "minute", "miss", "mission", "model", "modern", "moment", "money", "month", "more", "morning", "most", "mother", "mouth", "move", "movement", "movie", "Mr", "Mrs", "much", "music", "must", "my", "myself", "name", "nation", "national", "natural", "nature", "near", "nearly", "necessary", "need", "network", "never", "new", "news", "newspaper", "next", "nice", "night", "no", "none", "nor", "north", "not", "note", "nothing", "notice", "now", "n't", "number", "occur", "of", "off", "offer", "office", "officer", "official", "often", "oh", "oil", "ok", "old", "on", "once", "one", "only", "onto", "open", "operation", "opportunity", "option", "or", "order", "organization", "other", "others", "our", "out", "outside", "over", "own", "owner", "page", "pain", "painting", "paper", "parent", "part", "participant", "particular", "particularly", "partner", "party", "pass", "past", "patient", "pattern", "pay", "peace", "people", "per", "perform", "performance", "perhaps", "period", "person", "personal", "phone", "physical", "pick", "picture", "piece", "place", "plan", "plant", "play", "player", "PM", "point", "police", "policy", "political", "politics", "poor", "popular", "population", "position", "positive", "possible", "power", "practice", "prepare", "present", "president", "pressure", "pretty", "prevent", "price", "private", "probably", "problem", "process", "produce", "product", "production", "professional", "professor", "program", "project", "property", "protect", "prove", "provide", "public", "pull", "purpose", "push", "put", "quality", "question", "quickly", "quite", "race", "radio", "raise", "range", "rate", "rather", "reach", "read", "ready", "real", "reality", "realize", "really", "reason", "receive", "recent", "recently", "recognize", "record", "red", "reduce", "reflect", "region", "relate", "relationship", "religious", "remain", "remember", "remove", "report", "represent", "Republican", "require", "research", "resource", "respond", "response", "responsibility", "rest", "result", "return", "reveal", "rich", "right", "rise", "risk", "road", "rock", "role", "room", "rule", "run", "safe", "same", "save", "say", "scene", "school", "science", "scientist", "score", "sea", "season", "seat", "second", "section", "security", "see", "seek", "seem", "sell", "send", "senior", "sense", "series", "serious", "serve", "service", "set", "seven", "several", "sex", "sexual", "shake", "share", "she", "shoot", "short", "shot", "should", "shoulder", "show", "side", "sign", "significant", "similar", "simple", "simply", "since", "sing", "single", "sister", "sit", "site", "situation", "six", "size", "skill", "skin", "small", "smile", "so", "social", "society", "soldier", "some", "somebody", "someone", "something", "sometimes", "son", "song", "soon", "sort", "sound", "source", "south", "southern", "space", "speak", "special", "specific", "speech", "spend", "sport", "spring", "staff", "stage", "stand", "standard", "star", "start", "state", "statement", "station", "stay", "step", "still", "stock", "stop", "store", "story", "strategy", "street", "strong", "structure", "student", "study", "stuff", "style", "subject", "success", "successful", "such", "suddenly", "suffer", "suggest", "summer", "support", "sure", "surface", "system", "table", "take", "talk", "task", "tax", "teach", "teacher", "team", "technology", "television", "tell", "ten", "tend", "term", "test", "than", "thank", "that", "the", "their", "them", "themselves", "then", "theory", "there", "these", "they", "thing", "think", "third", "this", "those", "though", "thought", "thousand", "threat", "three", "through", "throughout", "throw", "thus", "time", "to", "today", "together", "tonight", "too", "top", "total", "tough", "toward", "town", "trade", "traditional", "training", "travel", "treat", "treatment", "tree", "trial", "trip", "trouble", "true", "truth", "try", "turn", "TV", "two", "type", "under", "understand", "unit", "until", "up", "upon", "us", "use", "usually", "value", "various", "very", "victim", "view", "violence", "visit", "voice", "vote", "wait", "walk", "wall", "want", "war", "watch", "water", "way", "we", "weapon", "wear", "week", "weight", "well", "west", "western", "what", "whatever", "when", "where", "whether", "which", "while", "white", "who", "whole", "whom", "whose", "why", "wide", "wife", "will", "win", "wind", "window", "wish", "with", "within", "without", "woman", "wonder", "word", "work", "worker", "world", "worry", "would", "write", "writer", "wrong", "yard", "yeah", "year", "yes", "yet", "you", "young", "your", "yourself",
  "\n"
};

const char *lexicon_sperethiel[] = {
  "a", "aishar", "aynk", "Ar'laana", "Bele se'Farad", "belet", "Beletre", "Bratach Falan", "Bratach Gheal", "Brat'mael", "Cara'sir", "carromeleg", "carronasto", "cele", "celenit", "Celiste", "Ceneste", "cetheral", "Chaele ti'Desach", "chal'han", "Cinanestial", "cirolle", "co", "dae", "Dae'mistishsa", "daron", "delara", "draesis", "Draesis ti'Morel", "dresner", "ehlios", "eidolon", "elaishon", "Ele Arandur", "Eoerin", "faskit", "Glerethiel Morkhan Shoam", "goro", "goro'imri", "goronagee", "goronagit", "goronit", "heng", "heron", "Huro ke'Envar", "Iarmhidh", "im", "imiri", "Imiri ti-Versakhan", "irenis", "ke", "leäl", "laes", "laesal", "li", "llayah", "makkaherenit", "makkalos", "makkanagee", "masa'e", "Masae Seorach", "medaron", "mel'thelem", "meleg", "Mellakabal", "meraerth", "Meraerth ke'Tolo", "mes", "Mes ti'Cirolletishsa", "Mes ti'Maeaerthsa", "Mes ti'Perritaesa", "Mes ti'Raeghsa", "Mes ti'Telenetishsa", "milessaratish", "mis", "mistish", "Mistish Farad", "morel", "morkhan", "nage", "niach mawryn", "od", "ozidan", "pechet", "perest", "perritaesa", "qua", "raé", "raegh", "raén", "ranelles", "resp", "Respitish od Telenetish", "reth", "reth'im", "reve", "rillabothian", "rinelle", "Rinelle ke'Tesrae", "sa", "sallah", "Sa'mistishsa", "samriel", "se", "Se'har Maera", "semeraerthsa", "Se'ranshae Elenva", "serathilion", "serulos", "sersakhan", "se'-shepetra", "shatatain", "shay", "Shay ke'Sallah", "sielle", "speren", "sperethiel", "Taengele", "teheron", "tech", "teleg", "telene", "tesetilaro", "Tesetelinestea", "tesrae", "Tesrae k'Ailiu", "thelem", "thiel", "vereb'he", "veresp", "versakhan", "Versakhan ke'Raegh", "wineg", "zarien", "zathien", "akarenti", "bellaripila", "confectio", "custos", "fabrika", "ghareez", "ianatori", "karinthini", "kedate", "laverna", "nehr'esham", "praetor", "preces", "stipatori", "strategos", "thera",
  "\n"
};

const char *lexicon_spanish[] = {
  "spanish",
  "\n"
};

const char *lexicon_japanese[] = {
  "japanese",
  "\n"
};

const char *lexicon_chinese[] = {
  "yi", "shi", "bu", "le", "ren", "wo", "zai", "you", "ta", "zhe", "zhong", "da", "lai", "shang", "ge", "dao", "shuo", "men", "wei", "zi", "he", "ni", "di", "chu", "dao", "ye", "shii", "nian", "de", "jiu", "na", "yao", "xia", "yii", "sheng", "hui", "zii", "zhee", "qu", "zhi", "guo", "jia", "xue", "dui", "ke", "taa", "li", "hou", "xiao", "me", "xin", "duo", "tian", "er", "neng", "hao", "dou", "ran", "mei", "ri", "yu", "qi", "hai", "fa", "cheng", "shie", "yong", "zhu", "xing", "fang", "youu", "ru", "qian", "suo", "ben", "jian", "jing", "tou", "mian", "gong", "tong", "san", "yie", "lao", "cong", "dong", "liang", "chang",
  "\n"
};

const char *lexicon_korean[] = {
  "korean",
  "\n"
};

const char *lexicon_italian[] = {
  "italian",
  "\n"
};

const char *lexicon_russian[] = {
  "russian",
  "\n"
};

const char *lexicon_sioux[] = {
  "sioux",
  "\n"
};

const char *lexicon_makaw[] = {
  "makaw",
  "\n"
};

const char *lexicon_crow[] = {
  "crow",
  "\n"
};

const char *lexicon_salish[] = {
  "salish",
  "\n"
};

const char *lexicon_ute[] = {
  "ute",
  "\n"
};

const char *lexicon_navajo[] = {
  "navajo",
  "\n"
};

const char *lexicon_german[] = {
  "german",
  "\n"
};

const char *lexicon_orzet[] = {
  "orzet",
  "\n"
};

const char *lexicon_arabic[] = {
  "arabic",
  "\n"
};

const char *lexicon_latin[] = {
  "salvete", "omnes", "bardus", "brevis", "comminus", "electus", "extremus", "gravatus", "gravis", "hodiernus", "honorabilis", "idoneus", "ignarus", "ignavus", "ignotus", "immortalis", "incorruptus", "liquidus", "prudens", "regius", "rusticus", "salvus", "serius", "simplex", "tersus", "tutis", "vetus", "Bene", "bene", "benigne", "cras", "eminus", "graviter", "heri", "hodie", "pariter", "quoque", "quotiens", "semper", "serio", "velociter", "vero", "abbatia", "basium", "benevolentia", "caelum", "calamitas", "campana", "caput", "carmen", "commissum", "delectatio", "dux", "ecclesia", "episcopus", "epistula", "eventus", "famulus", "fides", "flamma", "frigus", "gaudium", "gloria", "gravitas", "hereditas", "hora", "hypocrita", "ictus", "ignis", "inceptum", "juvenis", "labor", "limen", "lingua", "linteum", "ludio", "ludus", "lues", "lux", "luxuria", "mane", "mare", "maritus", "mater", "materia", "matertera", "matrimonium", "memoria", "missa", "monachus", "mundus", "natio", "natura", "nefas", "nihilum", "novitas", "opera", "oratio", "palma", "pater", "pax", "placitum", "plorator", "ploratus", "pluvia", "pluma", "pulpa", "rectum", "sacrificum", "sapientia", "sermo", "servitus", "spes", "synagoga", "tabellae", "tempestas", "umbra", "uxor", "vestigium", "vita", "voluptas", "xiphias", "abduco", "adduco", "aegresco", "capto", "comminuo", "commisceo", "decerno", "defaeco", "degusto", "disputo", "dormio", "exerceo", "exspecto", "exstinguo", "exulto", "facio", "gusto", "ignoro", "ignosco", "imitor", "inscribo", "investigo", "lacesso", "lego", "leto", "libero", "ligo", "lino", "litigo", "ludo", "macto", "maero", "nego", "novo", "offero", "oro", "pando", "pario", "ploro", "quaeso", "renuo", "sanctifico", "sequor", "sero", "transeo", "vado", "a", "ab", "ad", "ante", "circum", "contra", "cum", "de", "e", "ex", "extra", "in", "inter", "intra", "ob", "per", "post", "prae", "praeter", "pro", "propter", "sine", "sub", "super", "trans", "versus", "a​ut", "et", "etiam", "neque", "quamquam", "quod", "sed", "tamen", "bella", "lingua", "gloriae", "omni", "spiritus", "sanctis", "horriblis", "bordus", "ignotus", "ignavus", "bene", "serio", "velociter", "vero", "cras", "deinde", "abbas", "basium", "caelum", "calamitas", "campana", "epistula", "carmen", "famulus", "ignis", "hereditas", "linteum", "ludio", "inceptum", "luxuria", "mane", "mare", "natio", "natura", "memoria", "oratio", "palma", "pax", "pluma", "spes", "umbra", "vita", "abduco", "decerno", "tamen", "et", "etiam", "neque", "aut",
  "\n"
};

const char *lexicon_gaelic[] = {
  "gaelic",
  "\n"
};

const char *lexicon_french[] = {
  "hon",
  "hon",
  "baguette",
  "fromage",
  "\n"
};
