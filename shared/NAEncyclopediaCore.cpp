// NAEncyclopediaCore: shared code for every NA Encyclopedia volume.
//
// Milestone 1 placeholder so the static library target has a source file.
// Real code goes in the subfolders:
//   content/   JSON loader, templating, goal lookup
//   analysis/  FIFOs, onset detector, loudness, metrics
//   chapters/  chapter state machine, feedback evaluation
//   ui/        book frame, spine, pages, knob, meter, buttons, LookAndFeel

namespace na
{
    int coreLibraryVersion() noexcept { return 1; }
}
