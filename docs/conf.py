# -*- coding: utf-8 -*-
#
# Configuration file for the Sphinx documentation builder.
#
# This file does only contain a selection of the most common options. For a
# full list see the documentation:
# http://www.sphinx-doc.org/en/stable/config

# -- Path setup --------------------------------------------------------------

# If extensions (or modules to document with autodoc) are in another directory,
# add these directories to sys.path here. If the directory is relative to the
# documentation root, use os.path.abspath to make it absolute, like shown here.
#
# import os
# import sys
# sys.path.insert(0, os.path.abspath('.'))


# -- Project information -----------------------------------------------------

project = 'sparrow-lang'
copyright = '2019, Lucian Radu Teodorescu'
author = 'Lucian Radu Teodorescu'

# The short X.Y version
version = ''
# The full version, including alpha/beta/rc tags
release = '0.10.33'


# -- General configuration ---------------------------------------------------

# If your documentation needs a minimal Sphinx version, state it here.
#
# needs_sphinx = '1.0'

# Add any Sphinx extension module names here, as strings. They can be
# extensions coming with Sphinx (named 'sphinx.ext.*') or your custom
# ones.
extensions = [
    'sphinx.ext.pngmath'
]
# Add any paths that contain templates here, relative to this directory.
templates_path = ['_templates']

# The suffix(es) of source filenames.
# You can specify multiple suffix as a list of string:
#
# source_suffix = ['.rst', '.md']
source_suffix = '.rst'

# The master toctree document.
master_doc = 'index'

# The language for content autogenerated by Sphinx. Refer to documentation
# for a list of supported languages.
#
# This is also used if you do content translation via gettext catalogs.
# Usually you set "language" from the command line for these cases.
language = None

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path .
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']

# The name of the Pygments (syntax highlighting) style to use.
# pygments_style = 'monokai'


# -- Options for HTML output -------------------------------------------------

# The theme to use for HTML and HTML Help pages.  See the documentation for
# a list of builtin themes.
#
html_theme = 'sphinx_rtd_theme'

# Theme options are theme-specific and customize the look and feel of a theme
# further.  For a list of options available for each theme, see the
# documentation.
#
# html_theme_options = {}

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
html_static_path = ['_static']

# Custom sidebar templates, must be a dictionary that maps document names
# to template names.
#
# The default sidebars (for documents that don't match any pattern) are
# defined by theme itself.  Builtin themes are using these templates by
# default: ``['localtoc.html', 'relations.html', 'sourcelink.html',
# 'searchbox.html']``.
#
# html_sidebars = {}


# -- Options for HTMLHelp output ---------------------------------------------

# Output file base name for HTML help builder.
htmlhelp_basename = 'sparrow-langdoc'


# -- Options for LaTeX output ------------------------------------------------

latex_elements = {
    # The paper size ('letterpaper' or 'a4paper').
    #
    # 'papersize': 'letterpaper',

    # The font size ('10pt', '11pt' or '12pt').
    #
    # 'pointsize': '10pt',

    # Additional stuff for the LaTeX preamble.
    #
    # 'preamble': '',

    # Latex figure (float) alignment
    #
    # 'figure_align': 'htbp',
}

# Grouping the document tree into LaTeX files. List of tuples
# (source start file, target name, title,
#  author, documentclass [howto, manual, or own class]).
latex_documents = [
    (master_doc, 'sparrow-lang.tex', 'sparrow-lang Documentation',
     'Lucian Radu Teodorescu', 'manual'),
]


# -- Options for manual page output ------------------------------------------

# One entry per manual page. List of tuples
# (source start file, name, description, authors, manual section).
man_pages = [
    (master_doc, 'sparrow-lang', 'sparrow-lang Documentation',
     [author], 1)
]


# -- Options for Texinfo output ----------------------------------------------

# Grouping the document tree into Texinfo files. List of tuples
# (source start file, target name, title, author,
#  dir menu entry, description, category)
texinfo_documents = [
    (master_doc, 'sparrow-lang', 'sparrow-lang Documentation',
     author, 'sparrow-lang', 'One line description of project.',
     'Miscellaneous'),
]


# -- Sparrow syntax ----------------------------------------------------------


from pygments.lexer import Lexer, RegexLexer, include, bygroups, using, \
     this, combined, inherit, do_insertions
from pygments.token import Text, Comment, Operator, Keyword, Name, String, \
     Number, Punctuation, Error, Literal, Generic
from pygments import token
from sphinx.highlighting import lexers

class SparrowLexer(RegexLexer):
    """
    Pygments lexer for Sparrow.
    """
    name = 'sparrow'
    aliases = ['Sparrow']
    filenames = '*.spr'

    # A basic ID
    _re_id = r'[a-zA-Z_][a-zA-Z0-9_]*(_[~!@#$%^&*+=|?:<>/-]+)?';
    # A qualified ID
    _re_qid = r'%s(\.%s)+' % (_re_id, _re_id);

    tokens = {
        'root': [
            include('colors'),
            include('whitespace'),
            include('decls'),
            include('constants'),
            include('general'),
        ],
        'whitespace': [
            (r'\n', Text),
            (r'\s+', Text),
            (r'\\\n', Text), # line continuation
            (r'//(\n|(.|\n)*?[^\\]\n)', Comment.Single),
            (r'/(\\\n)?[*](.|\n)*?[*](\\\n)?/', Comment.Multiline),
        ],
        'constants': [
            (r'"', String, 'string'),
            (r"'", String, 'sstring'),
            (r"<{", String, 'qstring'),
            (r'(\d+\.\d*|\.\d+|\d+)[eE][+-]?\d+[LlUu]*', Number.Float),
            (r'(\d+\.\d*|\.\d+|\d+[fF])[fF]?', Number.Float),
            (r'0x[0-9a-fA-F]+[LlUu]*', Number.Hex),
            (r'0[0-7]+[LlUu]*', Number.Oct),
            (r'\d+[LlUu]*', Number.Integer),
        ],
        'general': [
            (r'\[.+\]', Operator),  # Modifiers
            (r'\*/', Error),
            (r'[~!@#$%^&*+=|?:<>/\`\-\\]+', Operator),
            (r'[(),.{};]', Punctuation),
            (r'(module|import|package)\b', Keyword.Namespace),
            (r'(concept|datatype|fun|using|var)\b', Keyword),
            (r'(break|continue|for|if|return|while|else)\b', Keyword),
            (r'(false|null|true)\b', Keyword.Constant),
            (r'(Type|Uninitialized|Null|Bool|Byte|UByte|Short|UShort|Int|UInt|Long|ULong|'
             r'SizeType|DiffType|Float|Double|Char|StringRef|UntypedPtr)\b',
             Keyword.Type),
            (_re_id, Name)
        ],
        'decls': [
            include('whitespace'),
            # structure
            (r'(module)(\s+%s)' % _re_qid,
             bygroups(Keyword.Namespace, Name.Namespace)),
            (r'(import)(\s+%s)' % _re_qid,
             bygroups(Keyword.Namespace, Name.Namespace)),
            (r'(import)(\s+%s)' % _re_id,
             bygroups(Keyword.Namespace, Name.Namespace)),
            (r'(package)(\s+%s)' % _re_id,
             bygroups(Keyword.Namespace, Name.Namespace)),
            # concept & datatype & function & using & var
            (r'(concept)(\s+%s)' % _re_id,
             bygroups(Keyword.Declaration, Name.Class)),
            (r'(datatype)(\s+%s)' % _re_id,
             bygroups(Keyword.Declaration, Name.Class)),
            (r'(fun)(\s+(ctor|dtor|ctorFromCt)\b)',
             bygroups(Keyword.Declaration, Name.Function.Magic)),
            (r'(fun)(\s+%s)' % _re_id,
             bygroups(Keyword.Declaration, Name.Function)),
            (r'(var)(\s+{0}(\s*,\s*{0})*)'.format(_re_id),
             bygroups(Keyword.Declaration, Name.Variable)),
        ],
        'string': [
            (r'"', String, '#pop'),
            (r'\\([\\abfnrtv"\']|x[a-fA-F0-9]{2,4}|'
             r'u[a-fA-F0-9]{4}|U[a-fA-F0-9]{8}|[0-7]{1,3})', String.Escape),
            (r'[^\\"\n]+', String), # all other characters
            (r'\\\n', String), # line continuation
            (r'\\', String), # stray backslash
        ],
        'sstring': [
            (r"'", String, '#pop'),
            (r'\\([\\abfnrtv"\']|x[a-fA-F0-9]{2,4}|'
             r'u[a-fA-F0-9]{4}|U[a-fA-F0-9]{8}|[0-7]{1,3})', String.Escape),
            (r'[^\\\'\n]+', String), # all other characters
            (r'\\\n', String), # line continuation
            (r'\\', String), # stray backslash
        ],
        'qstring': [
            (r'}>', String, '#pop'),
        ],
        'colors': [
            # ('Generic.Traceback', Generic.Traceback),
            # ('Generic.Subheading', Generic.Subheading),
            # ('Generic.Strong', Generic.Strong),
            # ('Generic.Prompt', Generic.Prompt),
            # ('Generic.Output', Generic.Output),
            # ('Generic.Inserted', Generic.Inserted),
            # ('Generic.Heading', Generic.Heading),
            # ('Generic.Error', Generic.Error),
            # ('Generic.Emph', Generic.Emph),
            # ('Generic.Deleted', Generic.Deleted),
            # ('Generic', Generic),
            # ('Comment.Special', Comment.Special),
            # ('Comment.Single', Comment.Single),
            # ('Comment.Preproc', Comment.Preproc),
            # ('Comment.Multiline', Comment.Multiline),
            # ('Comment.Hashbang', Comment.Hashbang),
            # ('Comment', Comment),
            # ('Punctuation', Punctuation),
            # ('Operator.Word', Operator.Word),
            # ('Operator', Operator),
            # ('Number.Oct', Number.Oct),
            # ('Number.Integer.Long', Number.Integer.Long),
            # ('Number.Integer', Number.Integer),
            # ('Number.Hex', Number.Hex),
            # ('Number.Float', Number.Float),
            # ('Number.Bin', Number.Bin),
            # ('Number', Number),
            # ('String.Symbol', String.Symbol),
            # ('String.Single', String.Single),
            # ('String.Regex', String.Regex),
            # ('String.Other', String.Other),
            # ('String.Interpol', String.Interpol),
            # ('String.Heredoc', String.Heredoc),
            # ('String.Escape', String.Escape),
            # ('String.Double', String.Double),
            # ('String.Doc', String.Doc),
            # ('String.Delimiter', String.Delimiter),
            # ('String.Char', String.Char),
            # ('String.Backtick', String.Backtick),
            # ('String.Affix', String.Affix),
            # ('String', String),
            # ('Literal.Date', Literal.Date),
            # ('Literal', Literal),
            # ('Name.Variable.Magic', Name.Variable.Magic),
            # ('Name.Variable.Instance', Name.Variable.Instance),
            # ('Name.Variable.Global', Name.Variable.Global),
            # ('Name.Variable.Class', Name.Variable.Class),
            # ('Name.Variable', Name.Variable),
            # ('Name.Tag', Name.Tag),
            # ('Name.Other', Name.Other),
            # ('Name.Namespace', Name.Namespace),
            # ('Name.Label', Name.Label),
            # ('Name.Function.Magic', Name.Function.Magic),
            # ('Name.Function', Name.Function),
            # ('Name.Exception', Name.Exception),
            # ('Name.Entity', Name.Entity),
            # ('Name.Decorator', Name.Decorator),
            # ('Name.Constant', Name.Constant),
            # ('Name.Class', Name.Class),
            # ('Name.Builtin.Pseudo', Name.Builtin.Pseudo),
            # ('Name.Builtin', Name.Builtin),
            # ('Name.Attribute', Name.Attribute),
            # ('Name', Name),
            # ('Keyword.Type', Keyword.Type),
            # ('Keyword.Reserved', Keyword.Reserved),
            # ('Keyword.Pseudo', Keyword.Pseudo),
            # ('Keyword.Namespace', Keyword.Namespace),
            # ('Keyword.Declaration', Keyword.Declaration),
            # ('Keyword.Constant', Keyword.Constant),
            # ('Keyword', Keyword),
        ],
    }

lexers['sparrow'] = SparrowLexer(startinline=True)
