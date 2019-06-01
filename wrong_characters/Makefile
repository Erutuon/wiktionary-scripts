template_names = l langname-mention m t t+ t+check t-check \
	borrowed cognate derived inherited noncognate \
	affix confix prefix suffix

template_dir = ../templates/
template_files = $(foreach template_name,$(template_names),$(template_dir)$(template_name).txt)

all:
	cat $(template_files) | LUA_PATH='../lua/?.lua;;' lua ../lua/wrong_characters_in_link_templates.lua