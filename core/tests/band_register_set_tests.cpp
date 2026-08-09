#include <gui/widgets/band_stack.h>
#include <stdexcept>

namespace {

    BandRegister reg(double frequency) {
        return { frequency, RADIO_IFACE_MODE_USB };
    }

    void expect(bool condition, const char* message) {
        if (!condition) { throw std::runtime_error(message); }
    }

    void expectFrequency(
        const BandRegisterSet& registers,
        std::size_t index,
        double frequency)
    {
        expect(registers[index].has_value(), "expected populated register");
        expect(registers[index]->freq == frequency, "unexpected frequency");
    }

    void testAppendAndCapacity() {
        BandRegisterSet registers;
        expect(registers.append(reg(1.0)), "first append failed");
        expect(registers.append(reg(2.0)), "second append failed");
        expect(registers.append(reg(3.0)), "third append failed");
        expect(!registers.append(reg(4.0)), "append exceeded fixed capacity");
        expectFrequency(registers, 0, 1.0);
        expectFrequency(registers, 1, 2.0);
        expectFrequency(registers, 2, 3.0);
    }

    void testCycleUsesOnlyPopulatedPrefix() {
        BandRegisterSet one;
        one.append(reg(1.0));
        one.cyclePopulated();
        expectFrequency(one, 0, 1.0);
        expect(!one[1], "single-entry cycle pushed an empty slot");

        BandRegisterSet two;
        two.append(reg(1.0));
        two.append(reg(2.0));
        two.cyclePopulated();
        expectFrequency(two, 0, 2.0);
        expectFrequency(two, 1, 1.0);
        expect(!two[2], "two-entry cycle pushed an empty slot");

        BandRegisterSet three;
        three.append(reg(1.0));
        three.append(reg(2.0));
        three.append(reg(3.0));
        three.cyclePopulated();
        expectFrequency(three, 0, 3.0);
        expectFrequency(three, 1, 1.0);
        expectFrequency(three, 2, 2.0);
    }

    void testSelectAndMaterialize() {
        BandRegisterSet populated;
        populated.append(reg(1.0));
        populated.append(reg(2.0));
        populated.append(reg(3.0));
        expect(populated.select(1).has_value(), "populated selection failed");
        expectFrequency(populated, 0, 2.0);
        expectFrequency(populated, 1, 1.0);
        expectFrequency(populated, 2, 3.0);

        BandRegisterSet sparse;
        sparse.append(reg(1.0));
        expect(!sparse.select(2), "empty selection unexpectedly succeeded");
        expectFrequency(sparse, 0, 1.0);
        expect(!sparse[1] && !sparse[2], "failed selection mutated the set");

        expect(sparse.select(2, reg(3.0)).has_value(),
               "empty selection was not materialized");
        expectFrequency(sparse, 0, 3.0);
        expectFrequency(sparse, 1, 1.0);
        expect(!sparse[2], "materialization left a gap in the prefix");
    }

}

int main() {
    testAppendAndCapacity();
    testCycleUsesOnlyPopulatedPrefix();
    testSelectAndMaterialize();
    return 0;
}
