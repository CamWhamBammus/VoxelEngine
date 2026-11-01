#include "skills.h"

#include <iostream>
#include <stdexcept>

using namespace voxel;
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
int main()
{
    try {
        check(skillTabNames().size() == 4, "four journal categories");
        for (int tab = 0; tab < 4; ++tab) {
            check(skillEntries(tab).size() == 6, "six readable ideas per page");
            for (const auto& skill : skillEntries(tab))
                check(!skill.name.empty() && !skill.description.empty(), "every placeholder has details");
        }
        check(skillEntries(-1).empty() && skillEntries(4).empty(), "invalid tabs have no entries");
        SkillJournal journal;
        journal.moveSelection({-1,-1});
        check(journal.selected == 0, "grid top-left does not wrap");
        journal.moveSelection({1,0}); journal.moveSelection({0,1});
        check(journal.selected == 4, "keyboard navigation follows the card grid");
        journal.selectSkill(5); journal.moveSelection({1,0});
        check(journal.selected == 5, "grid right edge does not wrap");
        journal.selectSkill(6); journal.selectSkill(-1);
        check(journal.selected == 5, "out-of-range clicks are ignored");
        journal.selectTab(1);
        check(journal.tab == 1 && journal.selected == 0, "a different page selects its first card");
        journal.selectSkill(2); journal.selectTab(1);
        check(journal.selected == 2, "clicking the same tab preserves selection");
        journal.selectTab(99); journal.selectTab(-1);
        check(journal.tab == 1 && journal.selected == 2, "invalid tabs preserve state");
        journal.selectTab(0); journal.cycleTab(-1);
        check(journal.tab == 3, "previous tab wraps to exploration");
        journal.cycleTab(1);
        check(journal.tab == 0, "next tab wraps to armor");
        std::cout << "Skill journal navigation passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
