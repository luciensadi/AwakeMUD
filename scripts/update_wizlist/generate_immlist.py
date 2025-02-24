# %%
import os
import textwrap

import mysql.connector

specific_folks = {
  "Production Staff": {
    "Coder": ["Lucien"],
    "Heads of Staff": ["Vile", "Jank"],
    "Helpers / Facilitators": ["Jexams", "Alak"],
  },
  "Buildport Staff": {
    "Head Builder": ["Vile (as Wither)"],
    "Builders": [
      "Pluto", # Diamond Chocolate
      "Stryker", # Cyber/bio and chargen shops
      "Nekryz", # Slitch Pit
      "MorbiusPhyre", # CAS
      "Warboss", # Horizon
      "Nerf", # clandestine cyberware
      "Epiphany", 
      "Neuromancer", # GDQ and matrix expansions
      "Ink", # White Flower Diner
      "Artemis",
      "Serge"
    ],
  },
}

ignored_characters = ["Wither", "Discord", "Aeternitas", "MikeTheLogger"]

# %%
# Fetch connection info from mysql_config.cpp
mysql_credentials = {}

for source_info_tuple in (
  ("main", os.path.join(os.path.expanduser("~"), "AwakeMUD", "src")),
  ("buildport", os.path.join(os.path.expanduser("~"), "buildport", "src"))
):
  cred_file_path = os.path.join(source_info_tuple[1], "mysql_config.cpp")
  
  print(f"Opening {cred_file_path}")
  
  with open(cred_file_path, "r") as creds:
    mysql_credentials[source_info_tuple[0]] = {}
    
    for line in creds.readlines():
      if not "const char *mysql_" in line:
          continue
      
      key = line.split("const char *mysql_")[1].split(" ")[0]
      value = line.split('"')[1].split('"')[0]
      
      mysql_credentials[source_info_tuple[0]][key] = value

mysql_credentials

# %%
# Code for adding staffers to the specific_folks list.

def add_staffer(name, db_slug):
  if name in ignored_characters:
    return
    
  if db_slug == "main":
    for k, v in specific_folks["Production Staff"].items():
      if name in v:
        return
    
    specific_folks["Production Staff"]["Helpers / Facilitators"].append(name)
  
  else:
    for k, v in specific_folks["Buildport Staff"].items():
      if name in v:
        return
    
    specific_folks["Buildport Staff"]["Builders"].append(name)

# %%
# Connect to each database and pull staff user info.

for db_slug, db_info_dict in mysql_credentials.items():
  cnx = mysql.connector.connect(
    user=db_info_dict['user'],
    password=db_info_dict['password'],
    host=db_info_dict['host'],
    database=db_info_dict['db']
  )
  
  cursor = cnx.cursor()
  query = ""
  
  if db_slug == "main":
    query = ("SELECT `name`, `rank` FROM pfiles WHERE `rank` > 1 AND `name` != 'deleted'")
  else:
    query = ("SELECT `name`, `rank` FROM pfiles WHERE `rank` > 1 AND `whotitle` = 'Builder' AND `name` != 'deleted'")
  
  cursor.execute(query)
  
  for name, rank in cursor:
    add_staffer(name, db_slug)

specific_folks

# %%
# Alphabetize and write out.

writeout = f"""
   ^W*********************************************************
   *^G              The Staff of AwakeMUD                  ^W*
   *********************************************************^n

                      ----------------
                      PRODUCTION STAFF
                      ----------------
"""

for title, names in specific_folks["Production Staff"].items():
  names = sorted(names)
  names = ", ".join(names)
  writeout += f"\r\n{title.upper().center(60)}"
  writeout += f"\r\n{names.center(60)}"
  writeout += "\r\n"
  
writeout += """
                      ---------------
                      BUILDPORT STAFF
                      ---------------
"""

for title, names in specific_folks["Buildport Staff"].items():
  names = sorted(names)
  names = ", ".join(names)
  wrapped_names = "\r\n".join([ln.center(60) for ln in textwrap.wrap(names, 60)])
  writeout += f"\r\n{title.upper().center(60)}"
  writeout += f"\r\n{wrapped_names.center(60)}"
  writeout += "\r\n"
  
with open("immlist", "w") as outfile:
  outfile.write(writeout)

writeout


