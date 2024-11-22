import { navbar } from "vuepress-theme-hope";

export default navbar([
  "/",
  {
    text: "文章",
    icon: "pen-to-square",
    prefix: "/posts/",
    children: [
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
