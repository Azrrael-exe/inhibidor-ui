# Uso:
#   make ui                      # levanta la UI de control (streamlit vía uv)
#   make stress                  # stress test contra $(IP) y reporte en reports/
#   make stress IP=192.168.1.50  # otro dispositivo
IP ?= 192.168.1.93
REPORT_DIR := reports

.PHONY: ui stress

ui:
	uv run ui.py

stress:
	@mkdir -p $(REPORT_DIR)
	@report=$(REPORT_DIR)/stress-$$(date +%Y%m%d-%H%M%S).txt; \
	echo "# Stress test — dispositivo $(IP) — $$(date '+%Y-%m-%d %H:%M:%S')" > $$report; \
	echo >> $$report; \
	if python3 tools/http_stress.py $(IP) >> $$report 2>&1; then verdict=PASS; else verdict=FAIL; fi; \
	echo >> $$report; \
	echo "RESULTADO: $$verdict" >> $$report; \
	cat $$report; \
	echo; \
	echo "Reporte guardado en: $$report"; \
	test $$verdict = PASS
