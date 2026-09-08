# lucrare — thesis sources

LaTeX sources of the thesis, written in Romanian, with an English abstract.

## Building

The document uses `fontspec`, so it requires **XeLaTeX or LuaLaTeX**. `pdflatex` fails
with a fatal package error.

```
latexmk -xelatex main.tex
```

Add `-g` to force a full rebuild if latexmk refuses to run after a previous failure.
The committed `main.pdf` is 30 pages.

## Structure

| File | Chapter |
|---|---|
| `main.tex` | Document root, layout and package setup |
| `0-title.tex` | Title page |
| `0-abstract.tex` | Abstract, Romanian and English; includes `declaratie.pdf` |
| `1-introducere.tex` | Introduction |
| `2-preliminarii.tex` | Background |
| `3-metode.tex` | Method |
| `4-rezultate.tex` | Results |
| `5-concluzii.tex` | Conclusions |
| `bibliography.bib` | References |
| `romanian.lbx` | Romanian localisation for biblatex |
| `images/` | Figures, photographs of the setup, confusion matrices |

`declaratie.pdf` is pulled into the document by `0-abstract.tex` through `pdfpages`; it
must stay in this directory or the build fails.

Build artefacts (`.aux`, `.log`, `.bbl`, `.fdb_latexmk`, `.fls`, `.xdv`, ...) are not
tracked. `main.pdf` is committed on purpose, so the thesis can be read directly on GitHub.
