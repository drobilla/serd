import os
import sys


from unittest.mock import Mock as MagicMock

sys.path.insert(0, os.path.abspath("../../build/bindings/python"))


class Mock(MagicMock):
    @classmethod
    def __getattr__(cls, name):
        return MagicMock()


MOCK_MODULES = ["cython", "libc.stdint"]
sys.modules.update((mod_name, Mock()) for mod_name in MOCK_MODULES)


# Project information

project = "Serd"
copyright = "2020, David Robillard"
author = "David Robillard"
release = "0.0.0"  # FIXME


# General configuration

exclude_patterns = ["xml", "_build", "Thumbs.db", ".DS_Store"]
language = "en"
nitpicky = True
pygments_style = "friendly"

extensions = [
    "sphinx.ext.autodoc",
    "sphinx.ext.doctest",
]

# Ignore everything opaque or external for nitpicky mode
_opaque = [
    "serd._SinkBase",
    "serd.__ByteSource",
    "unicode",
]

nitpick_ignore = list(map(lambda x: ("py:class", x), _opaque))

# HTML output

try:
    import sphinx_lv2_theme

    have_lv2_theme = True
except ModuleNotFoundError:
    have_lv2_theme = False

html_copy_source = False
html_short_title = "Serd"
html_static_path = ["_static"]
html_theme = "sphinx_lv2_theme"

if have_lv2_theme:
    html_theme = "sphinx_lv2_theme"

    if tags.has("singlehtml"):
        html_sidebars = {
            "**": [
                "globaltoc.html",
            ]
        }

        html_theme_options = {
            "body_max_width": "48em",
            "body_min_width": "48em",
            "description": "A lightweight library for working with RDF",
            "show_footer_version": True,
            "show_logo_version": False,
            "logo": "serd.svg",
            "logo_name": True,
            "logo_width": "8em",
            "nosidebar": False,
            "page_width": "80em",
            "sidebar_width": "18em",
            "globaltoc_maxdepth": 3,
            "globaltoc_collapse": False,
        }

    else:
        html_theme_options = {
            "body_max_width": "60em",
            "body_min_width": "40em",
            "description": "A lightweight library for working with RDF",
            "show_footer_version": True,
            "show_logo_version": False,
            "logo": "serd.svg",
            "logo_name": True,
            "logo_width": "8em",
            "nosidebar": True,
            "page_width": "60em",
            "sidebar_width": "14em",
            "globaltoc_maxdepth": 1,
            "globaltoc_collapse": True,
        }

else:

    html_theme = "alabaster"

    html_theme_options = {
        "body_max_width": "60em",
        "body_min_width": "40em",
        "description": "A lightweight library for working with RDF",
        "logo": "serd.svg",
        "logo_name": True,
        "page_width": "60em",
        "sidebar_width": "14em",
        "globaltoc_maxdepth": 1,
        "globaltoc_collapse": True,
    }
