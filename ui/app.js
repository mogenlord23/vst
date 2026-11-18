import * as Juce from "./juce/index.js";

const ping = Juce.getNativeFunction("ping");
document.getElementById("ping").addEventListener("click", async () => {
  const r = await ping("hello");
  document.getElementById("out").textContent = r;
});
