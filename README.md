# wiktionary-scripts

These are programs for processing the English Wiktionary dump.

`all-headers` generates data on the frequency of all headers by header level.

`filter-headers` generates a list of pages with the headers that they have. It is designed to be used with a large list of excluded headers, so it only allocates enough memory for four unique headers per page.

`find-templates` generates a list of (top-level) instances of a given template.

Both require [Expat](https://github.com/libexpat/libexpat), [Hat-Trie](https://github.com/dcjones/hat-trie), and [commander](https://github.com/clibs/commander).
