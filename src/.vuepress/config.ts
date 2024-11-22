import { defineUserConfig } from "vuepress";

import theme from "./theme.js";

export default defineUserConfig({
  base: "/",

  lang: "zh-CN",
  title: "叫完能的小熊猫",
  description: "个人博客",

  theme,

  // 和 PWA 一起启用
  // shouldPrefetch: false,
});
