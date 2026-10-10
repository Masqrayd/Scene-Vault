#include "../src/vault-state.hpp"
#include <cstdio>
#include <cstdlib>
static void check(bool ok, const char *message)
{
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
int main()
{
    VaultState state;
    const QSet<QString> scenes{"main", "camera", "overlay"};
    const QSet<QString> active{"main"};
    check(!state.mayHide("main", scenes, active), "active scene protected");
    check(!state.mayHide("missing", scenes, active), "nonexistent scene rejected");
    check(state.mayHide("camera", scenes, active), "inactive nested scene can be hidden");
    state.hidden.insert("camera");
    state.hidden.insert("overlay");
    check(!state.mayHide("main", scenes, {}), "last permanent visible scene protected");
    check(state.shouldHide("camera", active), "secondary hidden");
    check(!state.shouldHide("camera", {"camera"}), "external selection stays accessible");
    state.editing = "camera";
    check(!state.shouldHide("camera", active), "temporary editing visible");
    check(state.shouldHide("overlay", active), "other secondary remains hidden");
    state.suspended = true;
    check(!state.shouldHide("overlay", active), "recovery reveals all");
    state.resetSession();
    check(state.hidden.size() == 2 && state.shouldHide("camera", active), "session reset retains saved hidden membership");
    std::puts("PASS: vault state safeguards");
}
