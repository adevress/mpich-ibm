# Need to filter out a particular 3-line warning, and fail if any output remains.
# DOB: the translator output is now too messy to filter, so handle it at runtime
bug176c_st01: bug176c.upc
	$(UPCC) -o $@ $< >/dev/null 2>&1

