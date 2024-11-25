import { navbar } from "vuepress-theme-hope";

export default navbar([
  "/",
  {
    text: "文章",
    icon: "pen-to-square",
    prefix: "/",
    children: [
      {
        text: "DX12笔记",
        link: "posts/dx12/"
      },
      {
        text: "所有文章",
        link: "article/"
      }
    ],
  },
  {
    text: "时间线",
    icon: "timeline",
    link: "/timeline/"
  },{
    text: "关于",
    icon: "about",
    link: "intro.html"
  },
]);
