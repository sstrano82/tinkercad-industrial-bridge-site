# Tinkercad Industrial Bridge product page

This folder is a standalone GitHub Pages site. Publish its contents in a **separate public
repository** so the private/commercial bridge source code is not exposed.

## Publish with GitHub Pages

1. Create a new public GitHub repository, for example
   `tinkercad-industrial-bridge-site`.
2. Upload the contents of this folder to the repository root (the `index.html`,
   `examples` folder, and this README).
3. In the repository, open **Settings → Pages**.
4. Under **Build and deployment**, select **Deploy from a branch**, choose the `main`
   branch and the `/ (root)` folder, then save.
5. Once GitHub Pages finishes deploying, the page will be available at
   `https://sstrano82.github.io/tinkercad-industrial-bridge-site/`.

Do not copy the bridge's `src`, `server.js`, build output, or other proprietary software
files into the public site repository.

## Page content

- `index.html` is the product overview and includes feature descriptions, Tinkercad
  sample links, license tiers, and a placeholder for a future demo video.
- `examples/` contains the two downloadable Arduino sample sketches. The MIT license in
  that folder applies only to those example files, not to the commercial bridge.
- The system diagram is an inline SVG in `index.html`, so no image asset is needed.
- To add screenshots or a video later, upload them to an `assets/` folder in the Pages
  repository and link to them from `index.html`. For a hosted video, use its public
  embed URL; for a local video file, link to the uploaded MP4.

Review the product description, pricing, Tinkercad links, and license wording before
sharing the page with Lemon Squeezy.
