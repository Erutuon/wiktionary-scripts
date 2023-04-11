#! /usr/bin/env python3

from subprocess import run, PIPE
from os import environ
import os.path
import re
import orjson as json
from pywikibot import Site, Page
from traceback import print_exc

outdir = [ environ["HOME"], "pywikibot" ]
default_link_template_list = "link_templates.txt"

filters = {
    "ar": r"""regex [=[(?i)[aeiou]\u{304}?[aeiou]]=]:is_match(utf8proc.decomp(tr)) or not regex [=[^(?:[aeioug]\u{304}?|[ʾbjrʿfqlmnwyv]|k\u{331}?|h\u{323}?|[dt][\u{331}\u{323}]?|[sz][\u{323}\u{30C}]?|[\- ,;\.\?])+$]=]:is_match(utf8proc.decomp(tr))""",
    "arz": r"""tr:find "sh" or not regex [=[^[bptsgžḥḵdzrsšṣḍṭẓʿḡfvʾqklmnhwyaeiouāēīōū,\.\-,;?\s]+$]=]:is_match(tr) or regex [=[(?i)[aeiou]\u{304}?[aeiou]]=]:is_match(utf8proc.decomp(tr))""",
    "fa": r"""not regex [=[^[aâeoôêubptjčhxdzrzžsš\x27gğfqkglmnvuiwy \-,;\.?]+$]=]:is_match(tr)""",
}

# Prints template names from JSONL with schema of { "title": page_title, "templates": [ template_1, template_2, ... ]}

def get_all_templates(jsonl, format = "{{{{temp|{}}}}}"):
    data = dict()
    for line in jsonl.splitlines():
        try:
            d = json.loads(line)
        except Exception as e:
            raise TypeError(f"Could not load line {line}")
        data[d["title"]] = d["templates"]

    s = set()
    for k in data:
        for template in data[k]:
            s.add(re.match(r"\{\{(.+?)\|", template).group(1).strip())

    l = list(s)
    l.sort()
    return l

def filter_links(full_filter, *templates):
    args = [ "./lua/filter_link_templates.lua", full_filter ]
    args.extend(templates)
    process = run(args, stdout = PIPE, check = True, universal_newlines = True)
    return process.stdout

def repl_escape(s):
    return s.replace("\\", "\\\\")

def run_filter(lang_code, summary, force_run = False, minor = True, templates = []):
    filename = f"{lang_code}_bad_tr.jsonl"
    filepath = os.path.join(*outdir, filename)
    
    if lang_code not in filters:
        raise TypeError(f"lang_code {lang_code} doesn't have a filter")
    full_filter = f"""lang == "{lang_code}" and tr and ({filters[lang_code]})"""
    
    if force_run or not os.path.exists(filepath):
        jsonl = filter_links(full_filter, *templates)
        with open(filepath, "w") as f:
            f.write(jsonl)
    else:
        with open(filepath, "r") as f:
            jsonl = f.read()
    
    site = Site(code = "en", fam = "wiktionary")
    page = Page(site, title = f"User:Erutuon/lists/bad transliteration/{lang_code}")
    page.text = re.sub(
        r"""<source lang="lua">.+?</source>""",
        """<source lang="lua">\n""" + repl_escape(full_filter) + "\n</source>",
        page.text,
        flags = re.DOTALL)
    
    def set_text_in_markers(marker, text):
        page.text = re.sub(
            rf"<!--\s*start {marker}\s*-->.*?<!--\s*end {marker}\s*-->",
            f"<!-- start {marker} -->" + text + f"<!-- end {marker} -->",
            page.text,
            flags = re.DOTALL)
    
    def format_template(template_name):
        return "{{temp|" + template_name.replace("_", " ") + "}}"
    
    def format_templates(templates):
        return ", ".join(format_template(template) for template in templates)
    
    set_text_in_markers(
        "matching templates",
        format_templates(get_all_templates(jsonl)))
    
    all_templates = ([ line.strip() for line in open(default_link_template_list, "r") ]
        if len(templates) == 0
        else templates)
    all_templates.sort()
    set_text_in_markers("all templates", format_templates(all_templates))
    
    page.text = re.sub(
        r"""\{\{\s*#invoke:\s*User:Erutuon/10\s*\|\s*show\s*\|\s*<nowiki>.+?</nowiki>\s*\}\}""",
        "{{#invoke:User:Erutuon/10|show|\n<nowiki>\n" + repl_escape(jsonl.strip()) + "\n</nowiki>\n}}",
        page.text,
        flags = re.DOTALL)
    
    page.save(summary = summary, minor = minor)

try:
    date = "2022-01-01"
    run_filter(
        lang_code = "fa",
        force_run = True,
        summary = "update from " + date + " dump",
        minor = False,
        templates = "t t+ t-check t+check".split(" "))
    run_filter(
        lang_code = "ar",
        force_run = True,
        summary = "update from " + date + " dump",
        minor = False,
        templates = "affix antonyms back-formation blend borrowed calque circumfix clipping cognate compound confix coordinate_terms derived desc desctree deverbal doublet holonyms hypernyms hyponyms imperfectives inherited l l-self langname-mention learned_borrowing ll m m-self meronyms noncognate orthographic_borrowing perfectives phono-semantic_matching prefix rebracketing semantic_loan suffix synonyms t t+ t+check t-check transliteration troponyms".split(" "))
except Exception as e:
    print_exc()
