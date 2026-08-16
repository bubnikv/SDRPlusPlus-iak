#include <gui/widgets/band_stack.h>
#include <stdexcept>

namespace {

    BandRegister reg(double frequency, int mode) {
        return { frequency, mode };
    }

    void expect(bool condition, const char* message) {
        if (!condition) { throw std::runtime_error(message); }
    }

    void expectRegister(
        const BandRegisterSet& registers,
        std::size_t index,
        double frequency,
        int mode)
    {
        expect(registers[index].has_value(), "expected populated register");
        expect(registers[index]->freq == frequency, "unexpected frequency");
        expect(registers[index]->mode == mode, "unexpected mode");
    }

    void expectEmpty(const BandRegisterSet& registers, std::size_t index) {
        expect(!registers[index], "expected empty register");
    }

    BandRegisterSet fullStack() {
        BandRegisterSet registers;
        registers.setSlot(0, reg(1.0, RADIO_IFACE_MODE_NFM));
        registers.setSlot(1, reg(2.0, RADIO_IFACE_MODE_AM));
        registers.setSlot(2, reg(3.0, RADIO_IFACE_MODE_CW));
        return registers;
    }

    void testFixedSlotLoading() {
        BandRegisterSet registers;
        expect(registers.setSlot(0, reg(1.0, RADIO_IFACE_MODE_NFM)),
               "first slot failed");
        expect(registers.setSlot(2, reg(3.0, RADIO_IFACE_MODE_CW)),
               "third slot failed");
        expect(registers.size() == 3, "register capacity changed");
        expectRegister(registers, 0, 1.0, RADIO_IFACE_MODE_NFM);
        expectEmpty(registers, 1);
        expectRegister(registers, 2, 3.0, RADIO_IFACE_MODE_CW);
    }

    void testRepeatRotationContract() {
        const BandRegister current = reg(10.0, RADIO_IFACE_MODE_USB);

        BandRegisterSet empty;
        empty.repeatWithCurrent(current);
        expectRegister(empty, 0, 10.0, RADIO_IFACE_MODE_USB);
        expectEmpty(empty, 1);
        expectEmpty(empty, 2);

        BandRegisterSet full = fullStack();
        full.repeatWithCurrent(current);
        expectRegister(full, 0, 3.0, RADIO_IFACE_MODE_CW);
        expectRegister(full, 1, 10.0, RADIO_IFACE_MODE_USB);
        expectRegister(full, 2, 2.0, RADIO_IFACE_MODE_AM);

        BandRegisterSet emptyThird;
        emptyThird.setSlot(0, reg(1.0, RADIO_IFACE_MODE_NFM));
        emptyThird.setSlot(1, reg(2.0, RADIO_IFACE_MODE_AM));
        emptyThird.repeatWithCurrent(current);
        expectRegister(emptyThird, 0, 10.0, RADIO_IFACE_MODE_USB);
        expectRegister(emptyThird, 1, 1.0, RADIO_IFACE_MODE_NFM);
        expectRegister(emptyThird, 2, 2.0, RADIO_IFACE_MODE_AM);

        BandRegisterSet emptyMiddle;
        emptyMiddle.setSlot(0, reg(1.0, RADIO_IFACE_MODE_NFM));
        emptyMiddle.setSlot(2, reg(3.0, RADIO_IFACE_MODE_CW));
        emptyMiddle.repeatWithCurrent(current);
        expectRegister(emptyMiddle, 0, 10.0, RADIO_IFACE_MODE_USB);
        expectRegister(emptyMiddle, 1, 1.0, RADIO_IFACE_MODE_NFM);
        expectRegister(emptyMiddle, 2, 3.0, RADIO_IFACE_MODE_CW);

        BandRegisterSet onlyTop;
        onlyTop.setSlot(0, reg(1.0, RADIO_IFACE_MODE_NFM));
        onlyTop.repeatWithCurrent(current);
        expectRegister(onlyTop, 0, 10.0, RADIO_IFACE_MODE_USB);
        expectRegister(onlyTop, 1, 1.0, RADIO_IFACE_MODE_NFM);
        expectEmpty(onlyTop, 2);

        // Partial stacks fill without cycling; the following repeat starts the
        // full-stack roll and recalls the original seeded value.
        onlyTop.repeatWithCurrent(reg(20.0, RADIO_IFACE_MODE_LSB));
        expectRegister(onlyTop, 0, 20.0, RADIO_IFACE_MODE_LSB);
        expectRegister(onlyTop, 1, 10.0, RADIO_IFACE_MODE_USB);
        expectRegister(onlyTop, 2, 1.0, RADIO_IFACE_MODE_NFM);
        onlyTop.repeatWithCurrent(reg(30.0, RADIO_IFACE_MODE_AM));
        expectRegister(onlyTop, 0, 1.0, RADIO_IFACE_MODE_NFM);
        expectRegister(onlyTop, 1, 30.0, RADIO_IFACE_MODE_AM);
        expectRegister(onlyTop, 2, 10.0, RADIO_IFACE_MODE_USB);

        BandRegisterSet onlyMiddle;
        onlyMiddle.setSlot(1, reg(2.0, RADIO_IFACE_MODE_AM));
        onlyMiddle.repeatWithCurrent(current);
        expectRegister(onlyMiddle, 0, 10.0, RADIO_IFACE_MODE_USB);
        expectRegister(onlyMiddle, 1, 2.0, RADIO_IFACE_MODE_AM);
        expectEmpty(onlyMiddle, 2);

        BandRegisterSet noTop;
        noTop.setSlot(2, reg(3.0, RADIO_IFACE_MODE_CW));
        noTop.repeatWithCurrent(current);
        expectRegister(noTop, 0, 10.0, RADIO_IFACE_MODE_USB);
        expectEmpty(noTop, 1);
        expectRegister(noTop, 2, 3.0, RADIO_IFACE_MODE_CW);

        BandRegisterSet missingTop;
        missingTop.setSlot(1, reg(2.0, RADIO_IFACE_MODE_AM));
        missingTop.setSlot(2, reg(3.0, RADIO_IFACE_MODE_CW));
        missingTop.repeatWithCurrent(current);
        expectRegister(missingTop, 0, 10.0, RADIO_IFACE_MODE_USB);
        expectRegister(missingTop, 1, 2.0, RADIO_IFACE_MODE_AM);
        expectRegister(missingTop, 2, 3.0, RADIO_IFACE_MODE_CW);
    }

    void testRegisterSelectionContract() {
        // Slot 0 was captured when the popup opened. A populated selection
        // stores a newer state before promotion; an empty selection materializes
        // the newer state directly and preserves the captured top behind it.
        const BandRegister current = reg(10.0, RADIO_IFACE_MODE_USB);

        BandRegisterSet second = fullStack();
        second.storeTop(current);
        expect(second.select(1).has_value(), "second-register selection failed");
        expectRegister(second, 0, 2.0, RADIO_IFACE_MODE_AM);
        expectRegister(second, 1, 10.0, RADIO_IFACE_MODE_USB);
        expectRegister(second, 2, 3.0, RADIO_IFACE_MODE_CW);

        BandRegisterSet third = fullStack();
        third.storeTop(current);
        expect(third.select(2).has_value(), "third-register selection failed");
        expectRegister(third, 0, 3.0, RADIO_IFACE_MODE_CW);
        expectRegister(third, 1, 10.0, RADIO_IFACE_MODE_USB);
        expectRegister(third, 2, 2.0, RADIO_IFACE_MODE_AM);

        BandRegisterSet sparse;
        sparse.storeTop(current);
        sparse.setSlot(1, reg(2.0, RADIO_IFACE_MODE_AM));
        expect(!sparse.select(2), "empty selection unexpectedly succeeded");
        expectRegister(sparse, 0, 10.0, RADIO_IFACE_MODE_USB);
        expectRegister(sparse, 1, 2.0, RADIO_IFACE_MODE_AM);
        expectEmpty(sparse, 2);

        const BandRegister materialized = reg(20.0, RADIO_IFACE_MODE_LSB);
        expect(sparse.select(2, materialized).has_value(),
               "empty selection was not materialized");
        expectRegister(sparse, 0, 20.0, RADIO_IFACE_MODE_LSB);
        expectRegister(sparse, 1, 10.0, RADIO_IFACE_MODE_USB);
        expectRegister(sparse, 2, 2.0, RADIO_IFACE_MODE_AM);

        BandRegisterSet emptyMiddle;
        emptyMiddle.setSlot(0, reg(1.0, RADIO_IFACE_MODE_NFM));
        emptyMiddle.setSlot(2, reg(3.0, RADIO_IFACE_MODE_CW));
        expect(emptyMiddle.select(1, materialized).has_value(),
               "empty middle selection was not materialized");
        expectRegister(emptyMiddle, 0, 20.0, RADIO_IFACE_MODE_LSB);
        expectRegister(emptyMiddle, 1, 1.0, RADIO_IFACE_MODE_NFM);
        expectRegister(emptyMiddle, 2, 3.0, RADIO_IFACE_MODE_CW);

        BandRegisterSet onlyTop;
        onlyTop.setSlot(0, reg(1.0, RADIO_IFACE_MODE_NFM));
        expect(onlyTop.select(2, materialized).has_value(),
               "sparse empty third selection was not materialized");
        expectRegister(onlyTop, 0, 20.0, RADIO_IFACE_MODE_LSB);
        expectRegister(onlyTop, 1, 1.0, RADIO_IFACE_MODE_NFM);
        expectEmpty(onlyTop, 2);

        BandRegisterSet middleHole;
        middleHole.storeTop(current);
        middleHole.setSlot(2, reg(3.0, RADIO_IFACE_MODE_CW));
        expect(middleHole.select(2).has_value(), "sparse third selection failed");
        expectRegister(middleHole, 0, 3.0, RADIO_IFACE_MODE_CW);
        expectRegister(middleHole, 1, 10.0, RADIO_IFACE_MODE_USB);
        expectEmpty(middleHole, 2);
    }

    void testPopupOpenContract() {
        const BandRegister current = reg(10.0, RADIO_IFACE_MODE_USB);
        const BandRegister fallback = reg(20.0, RADIO_IFACE_MODE_AM);
        const BandRegisterSet stored = fullStack();

        const BandRegisterPopupPreparation active = prepareBandRegisterPopup(
            stored,
            current,
            fallback,
            true);
        expect(active.storedChanged,
               "active popup did not persist its current state");
        expectRegister(active.snapshot.registers, 0, 10.0, RADIO_IFACE_MODE_USB);
        expect(active.snapshot.canMaterializeEmpty,
               "in-band active popup disabled empty rows");

        const BandRegisterPopupPreparation inactiveOverlap =
            prepareBandRegisterPopup(stored, current, fallback, false);
        expect(!inactiveOverlap.storedChanged,
               "inactive overlapping popup overwrote a populated top");
        expectRegister(
            inactiveOverlap.snapshot.registers,
            0,
            1.0,
            RADIO_IFACE_MODE_NFM);
        expect(inactiveOverlap.snapshot.canMaterializeEmpty,
               "in-band overlapping popup disabled empty rows");

        BandRegisterSet emptyTop;
        emptyTop.setSlot(1, reg(2.0, RADIO_IFACE_MODE_AM));
        const BandRegisterPopupPreparation capturedCurrent =
            prepareBandRegisterPopup(emptyTop, current, fallback, true);
        expect(capturedCurrent.storedChanged,
               "empty active top did not capture in-band VFO");
        expectRegister(
            capturedCurrent.snapshot.registers,
            0,
            10.0,
            RADIO_IFACE_MODE_USB);

        const BandRegisterPopupPreparation inactiveFallback =
            prepareBandRegisterPopup(emptyTop, current, fallback, false);
        expect(inactiveFallback.storedChanged,
               "empty inactive top did not use fallback");
        expectRegister(
            inactiveFallback.snapshot.registers,
            0,
            20.0,
            RADIO_IFACE_MODE_AM);
        expect(inactiveFallback.snapshot.canMaterializeEmpty,
               "overlapping inactive popup disabled empty rows");

        const BandRegisterPopupPreparation unseeded = prepareBandRegisterPopup(
            emptyTop,
            std::nullopt,
            std::nullopt,
            false);
        expect(!unseeded.storedChanged,
               "popup seeded without a valid source");
        expectEmpty(unseeded.snapshot.registers, 0);
    }

}

int main() {
    testFixedSlotLoading();
    testRepeatRotationContract();
    testRegisterSelectionContract();
    testPopupOpenContract();
    return 0;
}
