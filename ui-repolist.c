/* ui-repolist.c: functions for generating the repolist page
 *
 * Copyright (C) 2006-2014 cgit Development Team <cgit@lists.zx2c4.com>
 *
 * Licensed under GNU General Public License v2
 *   (see COPYING for full license text)
 */

#include "cgit.h"
#include "ui-repolist.h"
#include "html.h"
#include "ui-shared.h"

static time_t read_agefile(char *path)
{
	time_t result;
	size_t size;
	char *buf = NULL;
	struct strbuf date_buf = STRBUF_INIT;

	if (readfile(path, &buf, &size)) {
		free(buf);
		return -1;
	}

	if (parse_date(buf, &date_buf) == 0)
		result = strtoul(date_buf.buf, NULL, 10);
	else
		result = 0;
	free(buf);
	strbuf_release(&date_buf);
	return result;
}

static int get_repo_modtime(const struct cgit_repo *repo, time_t *mtime)
{
	struct strbuf path = STRBUF_INIT;
	struct stat s;
	struct cgit_repo *r = (struct cgit_repo *)repo;

	if (repo->mtime != -1) {
		*mtime = repo->mtime;
		return 1;
	}
	strbuf_addf(&path, "%s/%s", repo->path, ctx.cfg.agefile);
	if (stat(path.buf, &s) == 0) {
		*mtime = read_agefile(path.buf);
		if (*mtime) {
			r->mtime = *mtime;
			goto end;
		}
	}

	strbuf_reset(&path);
	strbuf_addf(&path, "%s/refs/heads/%s", repo->path,
		    repo->defbranch ? repo->defbranch : "master");
	if (stat(path.buf, &s) == 0) {
		*mtime = s.st_mtime;
		r->mtime = *mtime;
		goto end;
	}

	strbuf_reset(&path);
	strbuf_addf(&path, "%s/%s", repo->path, "packed-refs");
	if (stat(path.buf, &s) == 0) {
		*mtime = s.st_mtime;
		r->mtime = *mtime;
		goto end;
	}

	*mtime = 0;
	r->mtime = *mtime;
end:
	strbuf_release(&path);
	return (r->mtime != 0);
}

static void print_modtime(struct cgit_repo *repo)
{
	time_t t;
	if (get_repo_modtime(repo, &t))
		cgit_print_age(t, 0, -1);
}

static int is_match(struct cgit_repo *repo)
{
	if (!ctx.qry.search)
		return 1;
	if (repo->url && strcasestr(repo->url, ctx.qry.search))
		return 1;
	if (repo->name && strcasestr(repo->name, ctx.qry.search))
		return 1;
	if (repo->desc && strcasestr(repo->desc, ctx.qry.search))
		return 1;
	if (repo->owner && strcasestr(repo->owner, ctx.qry.search))
		return 1;
	return 0;
}

static int is_in_url(struct cgit_repo *repo)
{
	if (!ctx.qry.url)
		return 1;
	if (repo->url && starts_with(repo->url, ctx.qry.url))
		return 1;
	return 0;
}

static int is_visible(struct cgit_repo *repo)
{
	if (repo->hide || repo->ignore)
		return 0;
	if (!(is_match(repo) && is_in_url(repo)))
		return 0;
	return 1;
}

static int any_repos_visible(void)
{
	int i;

	for (i = 0; i < cgit_repolist.count; i++) {
		if (is_visible(&cgit_repolist.repos[i]))
			return 1;
	}
	return 0;
}

static void print_sort_header(const char *title, const char *sort)
{
	char *currenturl = cgit_currenturl();
	html("<th class='left'><a href='");
	html_attr(currenturl);
	htmlf("?s=%s", sort);
	if (ctx.qry.search) {
		html("&amp;q=");
		html_url_arg(ctx.qry.search);
	}
	htmlf("'>%s</a></th>", title);
	free(currenturl);
}

static void print_header(void)
{
	html("<tr class='nohover'>");
	print_sort_header("Name", "name");
	print_sort_header("Description", "desc");
	if (ctx.cfg.enable_index_owner)
		print_sort_header("Owner", "owner");
	print_sort_header("Idle", "idle");
	if (ctx.cfg.enable_index_links)
		html("<th class='left'>Links</th>");
	html("</tr>\n");
}


static void print_pager(int items, int pagelen, char *search, char *sort)
{
	int i, ofs;
	char *class = NULL;
	html("<ul class='pager'>");
	for (i = 0, ofs = 0; ofs < items; i++, ofs = i * pagelen) {
		class = (ctx.qry.ofs == ofs) ? "current" : NULL;
		html("<li>");
		cgit_index_link(fmt("[%d]", i + 1), fmt("Page %d", i + 1),
				class, search, sort, ofs, 0);
		html("</li>");
	}
	html("</ul>");
}

static int cmp(const char *s1, const char *s2)
{
	if (s1 && s2) {
		if (ctx.cfg.case_sensitive_sort)
			return strcmp(s1, s2);
		else
			return strcasecmp(s1, s2);
	}
	if (s1 && !s2)
		return -1;
	if (s2 && !s1)
		return 1;
	return 0;
}

static int sort_section(const void *a, const void *b)
{
	const struct cgit_repo *r1 = a;
	const struct cgit_repo *r2 = b;
	int result;
	time_t t;

	result = cmp(r1->section, r2->section);
	if (!result) {
		if (!strcmp(ctx.cfg.repository_sort, "age")) {
			// get_repo_modtime caches the value in r->mtime, so we don't
			// have to worry about inefficiencies here.
			if (get_repo_modtime(r1, &t) && get_repo_modtime(r2, &t))
				result = r2->mtime - r1->mtime;
		}
		if (!result)
			result = cmp(r1->name, r2->name);
	}
	return result;
}

static int sort_name(const void *a, const void *b)
{
	const struct cgit_repo *r1 = a;
	const struct cgit_repo *r2 = b;

	return cmp(r1->name, r2->name);
}

static int sort_desc(const void *a, const void *b)
{
	const struct cgit_repo *r1 = a;
	const struct cgit_repo *r2 = b;

	return cmp(r1->desc, r2->desc);
}

static int sort_owner(const void *a, const void *b)
{
	const struct cgit_repo *r1 = a;
	const struct cgit_repo *r2 = b;

	return cmp(r1->owner, r2->owner);
}

static int sort_idle(const void *a, const void *b)
{
	const struct cgit_repo *r1 = a;
	const struct cgit_repo *r2 = b;
	time_t t1, t2;

	t1 = t2 = 0;
	get_repo_modtime(r1, &t1);
	get_repo_modtime(r2, &t2);
	return t2 - t1;
}

struct sortcolumn {
	const char *name;
	int (*fn)(const void *a, const void *b);
};

static const struct sortcolumn sortcolumn[] = {
	{"section", sort_section},
	{"name", sort_name},
	{"desc", sort_desc},
	{"owner", sort_owner},
	{"idle", sort_idle},
	{NULL, NULL}
};

static int sort_repolist(char *field)
{
	const struct sortcolumn *column;

	for (column = &sortcolumn[0]; column->name; column++) {
		if (strcmp(field, column->name))
			continue;
		qsort(cgit_repolist.repos, cgit_repolist.count,
			sizeof(struct cgit_repo), column->fn);
		return 1;
	}
	return 0;
}

struct repolist_ctx
{
	/* From outer contex passed to interior */
	int columns;
	int sorted;

	/* Used in interior context; should be reset in repolist_walk_visible() */
	int hits;
	const char *last_section;
	int section_cnt;
	int section_nested_cnt;
};

static void html_section(struct cgit_repo *repo, int columns)
{
	htmlf("<tr class='nohover'><td colspan='%d' class='reposection'>",
		columns);
	if (ctx.cfg.section_filter)
		cgit_open_filter(ctx.cfg.section_filter);
	html_txt(repo->section);
	if (ctx.cfg.section_filter)
		cgit_close_filter(ctx.cfg.section_filter);
	html("</td></tr>");
}

static void html_repository(struct cgit_repo *repo, int sorted)
{
	bool is_toplevel;

	ctx.repo = repo;

	is_toplevel = (!sorted && NULL != repo->section && repo->section[0] != '\0');
	htmlf("<tr><td class='%s'>", is_toplevel ? "sublevel-repo" : "toplevel-repo");
	cgit_summary_link(repo->name, repo->name, NULL, NULL);
	html("</td><td>");
	html_link_open(cgit_repourl(repo->url), NULL, NULL);
	html_ntxt(ctx.cfg.max_repodesc_len, repo->desc);
	html_link_close();
	html("</td><td>");
	if (ctx.cfg.enable_index_owner) {
		if (repo->owner_filter) {
			cgit_open_filter(repo->owner_filter);
			html_txt(repo->owner);
			cgit_close_filter(repo->owner_filter);
		} else {
			html("<a href='");
			html_attr(cgit_currenturl());
			html("?q=");
			html_url_arg(repo->owner);
			html("'>");
			html_txt(repo->owner);
			html("</a>");
		}
		html("</td><td>");
	}
	print_modtime(repo);
	html("</td>");
	if (ctx.cfg.enable_index_links) {
		html("<td>");
		cgit_summary_link("summary", NULL, "button", NULL);
		cgit_log_link("log", NULL, "button", NULL, NULL, NULL,
				0, NULL, NULL, ctx.qry.showmsg, 0);
		cgit_tree_link("tree", NULL, "button", NULL, NULL, NULL);
		html("</td>");
	}
	html("</tr>\n");
}

static inline bool should_emit_section(struct cgit_repo *repo, struct repolist_ctx *c)
{
	/* If we're sorted, we will not have a new section emitted. */
	if (c->sorted)
		return false;

	/* We need a valid repo section for the rest of the checks */
	if(NULL == repo->section)
		return false;

	/* If the section title is blank (e.g. top-level), we never emit
	 * a section heading. */
	if('\0' == repo->section[0])
		return false;

	/* Finally, compare the last section name to the current.  If they're
	 * the same, do not emit a section area. */
	if(NULL != c->last_section && !strcmp(repo->section, c->last_section))
		return false;

	c->last_section = repo->section;
	return true;
}

static int generate_repolist(struct cgit_repo *repo, struct repolist_ctx *c)
{
	c->hits++;
	if (c->hits <= ctx.qry.ofs)
		return 0;
	if (c->hits > ctx.qry.ofs + ctx.cfg.max_repo_count)
		return 0;

	if(should_emit_section(repo, c))
		html_section(repo, c->columns);
	html_repository(repo, c->sorted);

	return 0;
}

static int generate_sectionlist(struct cgit_repo *repo, struct repolist_ctx *c)
{
	bool is_toplevel;

	is_toplevel = (NULL == repo->section || repo->section[0] == '\0');

	if(!should_emit_section(repo, c) && !is_toplevel)
		return 0;

	c->hits++;

	if (c->hits <= ctx.qry.ofs)
		return 0;
	if (c->hits > ctx.qry.ofs + ctx.cfg.max_repo_count)
		return 0;

	if(is_toplevel)
		html_repository(repo, c->sorted);
	else
		html_section(repo, c->columns);

	return 0;
}

static int count_sections(struct cgit_repo *repo, struct repolist_ctx *c)
{
	const char *last_section;

	last_section = c->last_section;
	if(should_emit_section(repo, c))
	{
		c->section_cnt++;

		/* Determine if one section is nested within the other.  This
		 * is only accurate if this is a sorted list.
		 */
		if(NULL != last_section && NULL != repo->section)
		{
			int spn = strspn(last_section, repo->section);
			if(last_section[spn] == '\0')
			{
				c->section_nested_cnt++;
			}
		}
	}

	c->hits++;

	return 0;
}

typedef int (*repolist_walk_callback_t)(struct cgit_repo *repo, struct repolist_ctx *c);
static int repolist_walk_visible(repolist_walk_callback_t callback, struct repolist_ctx *c)
{
	struct cgit_repo *repo;
	int i;

	c->hits = 0;
	c->last_section = NULL;
	c->section_cnt = 0;
	c->section_nested_cnt = 0;

	for (i = 0; i < cgit_repolist.count; i++) {
		repo = &cgit_repolist.repos[i];
		if (!is_visible(repo))
			continue;
		if(NULL != callback)
			callback(repo, c);
	}
	return 0;
}

void cgit_print_repolist(void)
{
	bool section_pages = false;
	struct repolist_ctx repolist_ctx = {0};

	repolist_ctx.columns = 3;

	if (!any_repos_visible()) {
		cgit_print_error_page(404, "Not found", "No repositories found");
		return;
	}

	if (ctx.cfg.enable_index_links)
		++repolist_ctx.columns;
	if (ctx.cfg.enable_index_owner)
		++repolist_ctx.columns;

	ctx.page.title = ctx.cfg.root_title;
	cgit_print_http_headers();
	cgit_print_docstart();
	cgit_print_pageheader();

	if (ctx.cfg.index_header)
		html_include(ctx.cfg.index_header);

	if (ctx.qry.sort)
		repolist_ctx.sorted = sort_repolist(ctx.qry.sort);
	else if (ctx.cfg.section_sort)
	{
		sort_repolist("section");
		section_pages = (2 == ctx.cfg.section_sort);
	}

	html("<table summary='repository list' class='list nowrap'>");
	print_header();

	if(section_pages)
		repolist_walk_visible(count_sections, &repolist_ctx);

	if(section_pages && repolist_ctx.hits > ctx.cfg.max_repo_count &&
		repolist_ctx.section_cnt - repolist_ctx.section_nested_cnt > 1)
	{
		repolist_walk_visible(generate_sectionlist, &repolist_ctx);
	} else {
		repolist_walk_visible(generate_repolist, &repolist_ctx);
	}

	html("</table>");
	if (repolist_ctx.hits > ctx.cfg.max_repo_count)
		print_pager(repolist_ctx.hits, ctx.cfg.max_repo_count, ctx.qry.search, ctx.qry.sort);
	cgit_print_docend();
}

void cgit_print_site_readme(void)
{
	cgit_print_layout_start();
	if (!ctx.cfg.root_readme)
		goto done;
	cgit_open_filter(ctx.cfg.about_filter, ctx.cfg.root_readme);
	html_include(ctx.cfg.root_readme);
	cgit_close_filter(ctx.cfg.about_filter);
done:
	cgit_print_layout_end();
}
