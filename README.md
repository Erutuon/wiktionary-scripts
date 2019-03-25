# wiktionary-scripts

These are programs for processing the English Wiktionary dump.

`all-headers` generates data on the frequency of all headers by header level.

`filter-headers` generates a list of pages with the headers that they have. It is designed to be used with a large list of excluded headers, so it only allocates enough memory for four unique headers per page.

`find-templates` generates a list of all instances of a given template with the titles of the pages on which they were found.

`find-multiple-templates` is like `find-templates`, only it lists multiple templates.

All of them require [Expat](https://github.com/libexpat/libexpat) and [commander](https://github.com/clibs/commander), and `all-headers`, `filter-headers`, and `find-multiple-templates` also require [Hat-Trie](https://github.com/dcjones/hat-trie).
