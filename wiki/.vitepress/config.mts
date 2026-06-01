import { defineConfig } from 'vitepress';

export default defineConfig({
  title: 'MM2 Reverse Engineering',
  description: 'Might and Magic II (Amiga) — formats, runtime, tools, and editor docs',
  lang: 'en-US',
  appearance: 'force-dark',
  cleanUrls: true,
  lastUpdated: true,
  themeConfig: {
    logo: '/book-f00.png',
    nav: [
      { text: 'Home', link: '/' },
      { text: 'Overview', link: '/docs/reverse-engineering/00-overview' },
      { text: 'Gallery', link: '/docs/gallery/' },
      { text: 'Data Formats', link: '/docs/reverse-engineering/07-dat-files-and-formats' },
      { text: 'Editor', link: '/docs/editor/mm2ed' },
      { text: 'Tools', link: '/docs/tools/re-tools' },
    ],
    sidebar: {
      '/docs/': [
        {
          text: 'Getting Started',
          items: [
            { text: 'Project Overview', link: '/docs/reverse-engineering/00-overview' },
            { text: 'Workspace Notes', link: '/docs/reference/workspace-notes' },
            { text: 'Open Questions', link: '/docs/reverse-engineering/05-open-questions' },
            { text: 'Full Analysis', link: '/docs/reverse-engineering/mm2-ANALYSIS' },
          ],
        },
        {
          text: 'Runtime & Engine',
          collapsed: false,
          items: [
            { text: 'Startup & Init', link: '/docs/reverse-engineering/01-startup-init' },
            { text: 'Memory Map (A4)', link: '/docs/reverse-engineering/02-runtime-memory-map' },
            { text: 'Main Loop & Map', link: '/docs/reverse-engineering/03-main-loop-and-map' },
            { text: 'Party & Session', link: '/docs/reverse-engineering/04-party-and-session' },
            { text: 'Game State Struct', link: '/docs/reverse-engineering/14-game-state-struct' },
            { text: 'Time / Era / Calendar', link: '/docs/reverse-engineering/13-time-era-calendar' },
          ],
        },
        {
          text: 'Graphics & View',
          items: [
            { text: 'GFX Loading (.32)', link: '/docs/reverse-engineering/06-gfx-loading' },
            { text: 'ANM / TV Format', link: '/docs/reverse-engineering/07-anm-tv-format' },
            { text: '3D View & Game Screen', link: '/docs/reverse-engineering/15-3d-view-and-game-screen' },
          ],
        },
        {
          text: 'Data Formats (.dat)',
          collapsed: false,
          items: [
            { text: 'Format Inventory', link: '/docs/reverse-engineering/07-dat-files-and-formats' },
            { text: 'items.dat', link: '/docs/reverse-engineering/18-items-dat-format' },
            { text: 'monsters.dat', link: '/docs/reverse-engineering/16-monster-ability-format' },
            { text: 'roster.dat', link: '/docs/reverse-engineering/06-roster-format' },
            { text: 'map.dat', link: '/docs/reverse-engineering/03-main-loop-and-map' },
            { text: 'attrib.dat', link: '/docs/reverse-engineering/12-attrib-dat-format' },
            { text: 'spells.dat & item use', link: '/docs/reverse-engineering/19-spells-and-item-use' },
            { text: 'event.dat', link: '/docs/reverse-engineering/06-event-dat-format' },
            { text: 'Event Script Opcodes', link: '/docs/reverse-engineering/07-event-script-opcodes' },
          ],
        },
        {
          text: 'Game Systems',
          items: [
            { text: 'Combat System', link: '/docs/reverse-engineering/17-combat-system' },
            { text: 'Copy Protection', link: '/docs/reverse-engineering/20-copy-protection-table' },
          ],
        },
        {
          text: 'Sprite Gallery',
          collapsed: false,
          items: [
            { text: 'Gallery Home', link: '/docs/gallery/' },
            { text: 'Monsters', link: '/docs/gallery/monsters' },
            { text: 'Tilesets (.32)', link: '/docs/gallery/tilesets' },
            { text: 'Map Cartography', link: '/docs/gallery/maps' },
            { text: '3D View Graphics', link: '/docs/gallery/views' },
          ],
        },
        {
          text: 'Tools & Code',
          items: [
            { text: 'RE Tools (Python/C)', link: '/docs/tools/re-tools' },
            { text: 'C Decomp Scaffold', link: '/docs/decomp/readme' },
            { text: 'MM2ED Data Editor', link: '/docs/editor/mm2ed' },
          ],
        },
      ],
    },
    socialLinks: [
      { icon: 'github', link: 'https://github.com/Vairn/MM2' },
    ],
    search: { provider: 'local' },
    footer: {
      message: 'Might and Magic II (Amiga) reverse-engineering notes',
      copyright: 'Research documentation — not affiliated with New World Computing',
    },
  },
  head: [
    ['link', { rel: 'icon', href: '/book-f00.png', type: 'image/png' }],
  ],
});
