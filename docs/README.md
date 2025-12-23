# Documentation

This directory contains the source files for gSlapper's documentation, built with [MkDocs](https://www.mkdocs.org/) and the [Material theme](https://squidfunk.github.io/mkdocs-material/).

## Viewing Documentation

The documentation is automatically deployed to GitHub Pages:

**Live Site**: https://nomadcxx.github.io/gSlapper/

## Building Locally

To build and preview the documentation locally:

```bash
# Install dependencies
pip install mkdocs-material mkdocs-git-revision-date-localized-plugin pymdown-extensions

# Serve locally (auto-reloads on changes)
mkdocs serve

# Build static site
mkdocs build
```

The documentation will be available at `http://127.0.0.1:8000` when using `mkdocs serve`.

## Deployment

Documentation is automatically deployed via GitHub Actions when changes are pushed to the `development` or `main` branches. The workflow (`.github/workflows/docs.yml`) builds the site and deploys it to the `gh-pages` branch.

### Manual Deployment

If you need to deploy manually:

```bash
mkdocs gh-deploy
```

This will build the site and push it to the `gh-pages` branch.

## Structure

- `index.md` - Homepage
- `getting-started/` - Installation and setup guides
- `user-guide/` - Usage documentation
- `advanced/` - Advanced topics and troubleshooting
- `development/` - Developer documentation

## Editing

1. Edit the Markdown files in this directory
2. Test locally with `mkdocs serve`
3. Commit and push to `development` branch
4. GitHub Actions will automatically deploy
