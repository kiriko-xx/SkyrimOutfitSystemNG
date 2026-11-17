// This demo script will act like "Always use outfit" when
// the actor has a ring equipped. Otherwise, it just
// shows the equipped armor, as if SOS was disabled.

let name = "Require Ring";

fn select_armors(ctx, equipped, outfit) {
    if equipped[6].type_of() == "()" {
        return equipped;
    } else {
        return outfit;
    }
}
