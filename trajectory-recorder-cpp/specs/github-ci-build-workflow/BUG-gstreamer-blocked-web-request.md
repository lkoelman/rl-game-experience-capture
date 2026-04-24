When running the GitHub CI workflow, I get the following error during the Gstreamer installation phase:

```
Invoke-WebRequest: D:\a\_temp\d7c78e61-b9d5-4ee6-81ca-49f560359ecb.ps1:8
Line |
   8 |  Invoke-WebRequest `
     |  ~~~~~~~~~~~~~~~~~~~
     |                Checking you are not a bot
     | .dropdown:hover > .menu { display: block; }             .ui.secondary.menu .dropdown.item > .menu { margin-top:
     | 0; }
     | Checking you are not a bot                                                                Loading challenge
     | meta-refresh...                                                                    Why am I seeing this?
     | You are seeing this because the administrator of this website has set up go-away to protect the server against
     | the scourge of AI companies aggressively scraping websites.   Mass scraping can and does cause downtime for the
     | websites, which makes their resources inaccessible for everyone.   Please note that some challenges requires the
     | use of modern JavaScript features and some plugins may disable these. Disable such plugins for this domain (for
     | example, JShelter) if you encounter any issues.
     | Guru Meditation: b685a46764a3a8aaf7df30d213bb60a1
     | Protected by go-away :: Request Id b685a46764a3a8aaf7df30d213bb60a1
```

Suggest a solution.

Would it be practical to:
- host the installers ourselves, or store them somehow in this repository as an artefact?
    - cache the environment set up stages so we don't have to rerun them for each workflow run?
- build the runner image beforehand, with the environment and compiler toolchain already set up?

Think about multiple possible solutions, their pros and cons, and recommend one.