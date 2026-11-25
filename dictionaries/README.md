# Spell checking dictionaries

light_md checks spelling with [Hunspell](https://hunspell.github.io/) and these dictionaries
from the [LibreOffice dictionaries](https://github.com/LibreOffice/dictionaries) project
(commit `79195b0`). They are downloaded when light_md is built and shipped as separate files
next to the program; each one keeps its own license, which is not light_md's MIT license.

| Language | Files | License |
|---|---|---|
| English (US) | `en_US.aff`, `en_US.dic` | SCOWL: BSD-like, see [README_en_US.txt](https://github.com/LibreOffice/dictionaries/blob/79195b017e6d70151337856c59e7d13d311d6630/en/README_en_US.txt) |
| Español | `es_ES.aff`, `es_ES.dic` | GPL 3+, LGPL 3+ or MPL 1.1+, see [LICENSE.md](https://github.com/LibreOffice/dictionaries/blob/79195b017e6d70151337856c59e7d13d311d6630/es/LICENSE.md) |
| Português (Brasil) | `pt_BR.aff`, `pt_BR.dic` | LGPL 3 or MPL, see [README_pt_BR.txt](https://github.com/LibreOffice/dictionaries/blob/79195b017e6d70151337856c59e7d13d311d6630/pt_BR/README_pt_BR.txt) |
| Français | `fr.aff`, `fr.dic` | MPL 2.0, see [README_dict_fr.txt](https://github.com/LibreOffice/dictionaries/blob/79195b017e6d70151337856c59e7d13d311d6630/fr_FR/dictionaries/README_dict_fr.txt) |
| Deutsch | `de_DE_frami.aff`, `de_DE_frami.dic` | GPL 2 or 3, see [README_de_DE_frami.txt](https://github.com/LibreOffice/dictionaries/blob/79195b017e6d70151337856c59e7d13d311d6630/de/README_de_DE_frami.txt) |

Hunspell itself is used under the MPL 1.1 (it is also available under the GPL 2 and LGPL 2.1).

## More languages, or your own word list

Put any Hunspell dictionary (`xx_YY.aff` and `xx_YY.dic`) in light_md's dictionaries folder
in your user data (Options → Spelling → Open my dictionaries folder) and it replaces the one
for that language. Words you add with "Add to dictionary" are kept in `personal.txt` there,
one per line.
