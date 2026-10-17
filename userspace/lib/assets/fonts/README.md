# Desktop font source

DejaVu Sans from the Ubuntu `fonts-dejavu-core` package. The original
font is retained here to make atlas regeneration independent of host fonts.
See `LICENSE-DejaVu.txt` for redistribution terms.

SHA256: `b4c632e3cdf9acc7f28758fb5a323c8524d7fc6660d46904d9b6cbe2809c419c`

`python3 tools/make_desktop_font.py` generates the checked-in 16 px and 20 px
grayscale atlases using Pillow/FreeType. This generation used Pillow 12.1.1
and FreeType 2.14.2; different rasterizer versions may produce different pixels.
The ordinary OS build needs neither Pillow nor FreeType. Native `deskstudy --license`
prints the font notice, which is embedded with the atlas in the executable.
