-- This script is an example of an section-filter.  It replaces the
-- section title with several links for each subdirectory represented
-- within the section title.  (It's intended to be used where the section
-- title is also the same as the path to the repository, similar to
-- the output from the "section-from-path" option.)  This script may
-- be used with the section-filter setting in cgitrc with the `lua:`
-- prefix.

function gen_link(name, path)
end

function filter_open()
	buffer = ""
end

function filter_close()
	path = "/"
	cnt = 0
	for i in string.gmatch(buffer, "[^/]+") do
		if cnt > 0 then
			html("/")
		end
		path = path .. i .. "/"
		html(string.format("<a href=\"%s%s\" class=reposection>%s</a>",
			os.getenv("SCRIPT_NAME"), path, i))
		cnt = cnt + 1
	end
	return 0
end

function filter_write(str)
	buffer = buffer .. str
end
