report.pdf: report.typ codelinks.json
	typst compile report.typ report.pdf

codelinks.json:
	cd .. && python3 docs/genref.py

.PHONY: clean

clean:
	rm report.pdf codelinks.json
