PAGES_ARTICLES = ../pages-articles.xml

all-headers:
	../bin/all-headers -i $(PAGES_ARTICLES) -o all-headers.txt

filtered-headers:
	../bin/filter-headers \
		-i $(PAGES_ARTICLES) \
		-o filtered-headers.txt \
		-x correct_headers.txt \
		-x language_names.txt \
		-n '0 118'

language-names:
	lua -e 'for name in pairs(require "mediawiki.languages.name_to_code") do print(name) end' \
		> language_names.txt