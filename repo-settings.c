#include "cache.h"
#include "config.h"
#include "repository.h"
#include "midx.h"
#include "compat/fsmonitor/fsm-listen.h"

static void repo_cfg_bool(struct repository *r, const char *key, int *dest,
			  int def)
{
	if (repo_config_get_bool(r, key, dest))
		*dest = def;
}

static void repo_cfg_int(struct repository *r, const char *key, int *dest,
			 int def)
{
	if (repo_config_get_int(r, key, dest))
		*dest = def;
}

void prepare_repo_settings(struct repository *r)
{
	int experimental;
	int value;
	char *strval;
	int manyfiles;

	if (!r->gitdir)
		BUG("Cannot add settings for uninitialized repository");

	if (r->settings.initialized++)
		return;

	/* Defaults */
	r->settings.index_version = -1;
	r->settings.core_untracked_cache = UNTRACKED_CACHE_KEEP;
	r->settings.fetch_negotiation_algorithm = FETCH_NEGOTIATION_CONSECUTIVE;

	/* Booleans config or default, cascades to other settings */
	repo_cfg_bool(r, "feature.manyfiles", &manyfiles, 0);
	repo_cfg_bool(r, "feature.experimental", &experimental, 0);

	/* Defaults modified by feature.* */
	if (experimental) {
		r->settings.fetch_negotiation_algorithm = FETCH_NEGOTIATION_SKIPPING;
	}
	if (manyfiles) {
		r->settings.index_version = 4;
		r->settings.core_untracked_cache = UNTRACKED_CACHE_WRITE;
	}

	/* Commit graph config or default, does not cascade (simple) */
	repo_cfg_bool(r, "core.commitgraph", &r->settings.core_commit_graph, 1);
	repo_cfg_int(r, "commitgraph.generationversion", &r->settings.commit_graph_generation_version, 2);
	repo_cfg_bool(r, "commitgraph.readchangedpaths", &r->settings.commit_graph_read_changed_paths, 1);
	repo_cfg_bool(r, "gc.writecommitgraph", &r->settings.gc_write_commit_graph, 1);
	repo_cfg_bool(r, "fetch.writecommitgraph", &r->settings.fetch_write_commit_graph, 0);

	/* Boolean config or default, does not cascade (simple)  */
	repo_cfg_bool(r, "pack.usesparse", &r->settings.pack_use_sparse, 1);
	repo_cfg_bool(r, "core.multipackindex", &r->settings.core_multi_pack_index, 1);
	repo_cfg_bool(r, "index.sparse", &r->settings.sparse_index, 0);
	repo_cfg_bool(r, "submodule.propagateBranches", &r->settings.submodule_propagate_branches, 0);

	/*
	 * Boolean settings from config and environment variables. Only
	 * take the environment variable if it is true, otherwise, use
	 * the config.
	 */
	if (git_env_bool(GIT_TEST_MULTI_PACK_INDEX, 0))
		r->settings.core_multi_pack_index = 1;
	if (git_env_bool(GIT_SUBMODULE_PROPAGATE_BRANCHES_ENVIRONMENT, 0))
		r->settings.submodule_propagate_branches = 1;

	/*
	 * Non-boolean config
	 */
	if (!repo_config_get_int(r, "index.version", &value))
		r->settings.index_version = value;

	if (!repo_config_get_string(r, "core.untrackedcache", &strval)) {
		int v = git_parse_maybe_bool(strval);

		/*
		 * If it's set to "keep", or some other non-boolean
		 * value then "v < 0". Then we do nothing and keep it
		 * at the default of UNTRACKED_CACHE_KEEP.
		 */
		if (v >= 0)
			r->settings.core_untracked_cache = v ?
				UNTRACKED_CACHE_WRITE : UNTRACKED_CACHE_REMOVE;
		free(strval);
	}

	if (!repo_config_get_string(r, "fetch.negotiationalgorithm", &strval)) {
		int fetch_default = r->settings.fetch_negotiation_algorithm;
		if (!strcasecmp(strval, "skipping"))
			r->settings.fetch_negotiation_algorithm = FETCH_NEGOTIATION_SKIPPING;
		else if (!strcasecmp(strval, "noop"))
			r->settings.fetch_negotiation_algorithm = FETCH_NEGOTIATION_NOOP;
		else if (!strcasecmp(strval, "consecutive"))
			r->settings.fetch_negotiation_algorithm = FETCH_NEGOTIATION_CONSECUTIVE;
		else if (!strcasecmp(strval, "default"))
			r->settings.fetch_negotiation_algorithm = fetch_default;
		else
			die("unknown fetch negotiation algorithm '%s'", strval);
	}

	/*
	 * This setting guards all index reads to require a full index
	 * over a sparse index. After suitable guards are placed in the
	 * codebase around uses of the index, this setting will be
	 * removed.
	 */
	r->settings.command_requires_full_index = 1;
}
