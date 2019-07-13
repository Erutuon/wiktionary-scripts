template_names = back-formation desc l l-self langname-mention m m-self rebracketing t t+ t+check t-check \
	borrowed cognate derived inherited noncognate semantic_loan \
	affix confix prefix suffix

# template_names = back-formation desc semantic_loan rebracketing

# template_names = $(shell rg --replace='$$1' --only-matching '([^\n\t]+)\.txt$$' ../templates/link_templates.txt)

template_dir = ../templates/
template_files = $(foreach template_name,$(template_names),$(template_dir)$(template_name).txt)

all:
	cat $(template_files) | LUA_PATH='../lua/?.lua;;' lua ../lua/wrong_characters_in_link_templates.lua