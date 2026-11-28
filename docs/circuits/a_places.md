# Atlas level: a_places, a prediction bet (`circuits/a_places.json` = `circuits/a_places_given.json`)

Task `a_places` (`engine/src/tasks/a_places.cpp`, world in `atlas_compass.h`, chapter "a_compass", `level_kind` "diagnosis", order 996, off the spine, hangs off `a_signs`). The circuit is the compass with the signs and gains that hold a bump (the reference of `a_signs`, weights stored with their sign, no cell-type cards, nothing to edit).

## The bet
How many directions can this wiring hold? Candidates: `all16` (13 or more different resting headings), `about6` (4 to 9), `two` (1 to 3), `none` (no active trial). The level runs the sixteen cues one after the other (16 trials of 3,700 ms, a reset between them) and counts the headings the bump came to rest at: for each active trial (mean rate 0.3 Hz or more) the heading nearest to the angle of the bump in the last 500 ms. The player commits first (`Diagnosis::bet`, budget three instrument reads; the commit is sent with `set_variant`, see DESIGN-0.5.md); the run can be watched before the bet and each instrument read beyond the budget costs 3. The verdict names the count in words ("six headings").

## What passes
A commit naming a candidate whose range holds the count of the run. Measured, not stored: on circuit seeds 1 to 40 the count is six on 39 and five on 1 (so `about6`, 4 to 9, on 40 of 40; the other three candidates on 0 of 40). The resting headings are 2, 4, 6, 8, 11 and 15; cues 3 to 5 end at 4, 6 to 9 at 6 or 8, 10 to 13 at 11, 14 at 11 or 15, 15 and 0 at 15, 1 and 2 at 2 or 4 (research/atlas-a.md 4d). The cue at heading 13 never holds (it ends at 11, 45 degrees away). Value: 100 minus 15 per wrong commit and 3 per instrument read beyond the budget (floor 25).

## Honesty
Verdicts say "in this model". The count is a property of this model (point cells, fitted gains); whether the fly's compass has such places or a smooth ring that the model approximates is not settled (the notebook says so).
