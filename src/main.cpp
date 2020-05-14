#include <string>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ios>
#include <dqcsim>
#include "qasm_semantic.hpp"
#include "json.hpp"

// Alias the relevant namespaces to something shorter.
namespace dqcs = dqcsim::wrap;
namespace cq = compiler;

/**
 * Used to average measurements over time.
 */
class Measurement {
private:
  uint64_t num_total = 0;
  uint64_t num_one = 0;
  bool latest_value = false;
  nlohmann::json latest_data = "{}"_json;

public:
  /**
   * Adds a measurement sample.
   */
  void add(bool value, nlohmann::json &&data) {
    latest_value = value;
    latest_data = std::move(data);
    num_total++;
    if (value) num_one++;
  }

  /**
   * Returns the estimated probability that the measurement is a one, or -1 if
   * no estimation is available.
   */
  double p1() const {
    if (!num_total) {
      return -1.0;
    }
    return (double)num_one / (double)num_total;
  }

  /**
   * Returns the number of samples taken.
   */
  uint64_t num_samples() const {
    return num_total;
  }

  /**
   * Returns the latest measurement.
   */
  bool latest() const {
    return latest_value;
  }

  /**
   * Returns the extended JSON representation of the latest measurement.
   */
  nlohmann::json latest_json() const {
    return latest_data;
  }

  /**
   * Resets averaging.
   */
  void reset() {
    latest_value = false;
    num_total = 0;
    num_one = 0;
  }

};

/**
 * Main plugin class for QX.
 */
class CQASMPlugin {
public:

  /**
   * The path to the cQASM file.
   */
  const std::string filename;

  /**
   * The DQCsim qubits corresponding to the cQASM qubit indices.
   */
  std::vector<dqcs::QubitRef> qubits;

  /**
   * The cQASM classical register.
   */
  std::vector<bool> bits;

  /**
   * The cQASM measurement averaging register.
   */
  std::vector<Measurement> measurements;

  /**
   * Constructor.
   */
  CQASMPlugin(std::string &&filename) : filename(filename) {
  }

  /**
   * Interprets libQASM's brilliant operation representation into DQCsim's, and
   * executes it. Returns whether the operation takes time.
   */
  bool exec_gate(
    dqcs::RunningPluginState &state,
    const cq::Operation &operation
  ) {

    // Get the operation type.
    std::string type = operation.getType();
    DQCSIM_TRACE("operation: %s", type.c_str());

    // Handle conditionals.
    // NOTE: QX interprets gates with multiple conditionals and multiple qubits
    // such that all conditions need to be true for each parallel gate, not
    // each bit controlling each subgate. So we can just get conditions out of
    // the way immediately.
    if (operation.isBitControlled()) {
      for (auto bit_idx : operation.getControlBits().getSelectedBits().getIndices()) {
        if (!bits[bit_idx]) {
          return true;
        }
      }
    }

    // Handle prep gates.
    bool is_prep;
    dqcs::PauliBasis prep_basis = dqcs::PauliBasis::Z;
    if (type == "prep_x") {
      is_prep = true;
      prep_basis = dqcs::PauliBasis::X;
    }
    if (type == "prep_y") {
      is_prep = true;
      prep_basis = dqcs::PauliBasis::Y;
    }
    if (type == "prep_z") {
      is_prep = true;
    }
    if (is_prep) {
      dqcs::QubitSet qubit_refs = dqcs::QubitSet();
      for (auto qubit_idx : operation.getQubitsInvolved().getSelectedQubits().getIndices()) {
        qubit_refs.push(qubits[qubit_idx]);
      }
      state.gate(dqcs::Gate::prep(std::move(qubit_refs), prep_basis));
      return true;
    }

    // Handle measurements.
    bool is_measure = false;
    bool is_measure_all = false;
    dqcs::PauliBasis measure_basis = dqcs::PauliBasis::Z;
    if (type == "measure_x") {
      is_measure = true;
      measure_basis = dqcs::PauliBasis::X;
    }
    if (type == "measure_y") {
      is_measure = true;
      measure_basis = dqcs::PauliBasis::Y;
    }
    if (type == "measure_z" || type == "measure") {
      is_measure = true;
    }
    if (type == "measure_all") {
      is_measure = true;
      is_measure_all = true;
    }
    if (type == "measure_parity") {
      // FIXME
      is_measure = true;
      DQCSIM_ERROR("measure-parity is not implemented! interpreting as measure_z");
    }
    if (is_measure) {

      // Get the QX qubit indices.
      std::vector<size_t> qubit_idxs;
      if (is_measure_all) {
        for (size_t qubit_idx = 0; qubit_idx < qubits.size(); qubit_idx++) {
          qubit_idxs.push_back(qubit_idx);
        }
      } else {
        qubit_idxs = operation.getQubitsInvolved().getSelectedQubits().getIndices();
      }

      // Convert to DQCsim qubit indices.
      dqcs::QubitSet qubit_refs = dqcs::QubitSet();
      for (auto qubit_idx : qubit_idxs) {
        qubit_refs.push(qubits[qubit_idx]);
      }

      // Perform the measurement.
      state.gate(dqcs::Gate::measure(std::move(qubit_refs), measure_basis));

      // Read back the measurement results.
      for (auto qubit_idx : qubit_idxs) {
        bool value = false;
        nlohmann::json data = "{}"_json;
        auto measurement = state.get_measurement(qubits[qubit_idx]);
        auto raw = measurement.get_value();
        switch (raw) {
          case dqcs::MeasurementValue::Zero: value = false; data["raw"] = 0; break;
          case dqcs::MeasurementValue::One: value = true; data["raw"] = 1; break;
          case dqcs::MeasurementValue::Undefined:
            DQCSIM_WARN("Received undefined measurement for qubit %d, interpreting as 0", qubit_idx);
            data["raw"] = "null"_json;
            value = false;
            break;
        }
        data["json"] = measurement.get_arb_json<nlohmann::json>();
        auto binary_strings = "[]"_json;
        for (ssize_t i = 0; i < measurement.get_arb_arg_count(); i++) {
          auto str = measurement.get_arb_arg_string(i);
          std::vector<uint8_t> bytes(str.begin(), str.end());
          binary_strings.push_back(nlohmann::json(std::move(bytes)));
        }
        data["binary"] = std::move(binary_strings);
        bits[qubit_idx] = value;
        measurements[qubit_idx].add(value, std::move(data));
      }
      return true;
    }

    // Handle reset_averaging.
    if (type == "reset-averaging") {
      for (auto &measurement : measurements) {
        measurement.reset();
      }
      return false;
    }

    // Handle classical NOT.
    if (type == "not") {
      for (auto bit_idx : operation.getControlBits().getSelectedBits().getIndices()) {
        bits[bit_idx] = !bits[bit_idx];
      }
      return true;
    }

    // Handle display.
    if (type == "display") {
      static bool warned = false;
      if (!warned) {
        DQCSIM_WARN("DQCsim frontends cannot display qubit state; interpreting 'display' gates as 'display_binary'.");
        warned = true;
      }
      type = "display_binary";
    }
    if (type == "display_binary") {
      std::vector<size_t> idxs = operation.getControlBits().getSelectedBits().getIndices();
      if (!idxs.size()) {
        for (size_t qubit_idx = 0; qubit_idx < qubits.size(); qubit_idx++) {
          idxs.push_back(qubit_idx);
        }
      }

      for (int i : idxs) {
        if (measurements[i].num_samples()) {
          DQCSIM_INFO(
            "b%d: %d; q%d: %.6f (%d samples, latest = %d)",
            i, (int)bits[i], i,
            measurements[i].p1(),
            measurements[i].num_samples(),
            (int)measurements[i].latest());
        } else {
          DQCSIM_INFO(
            "b%d: %d; q%d: no data",
            i, (int)bits[i], i);
        }
      }
      return false;
    }

    // Handle delay.
    if (type == "wait") {
      state.advance(operation.getWaitTime());

      // Pretend that this operation doesn't take time; we've already advanced
      // all we needed to.
      return false;
    }

    // Handle pure-quantum gates.
    dqcs::PredefinedGate predef_gate;
    int num_targets = 0;
    bool has_angle = false;
    if (type == "i") {
      num_targets = 1;
      predef_gate = dqcs::PredefinedGate::I;
    }
    if (type == "x") {
      num_targets = 1;
      predef_gate = dqcs::PredefinedGate::X;
    }
    if (type == "y") {
      num_targets = 1;
      predef_gate = dqcs::PredefinedGate::Y;
    }
    if (type == "z") {
      num_targets = 1;
      predef_gate = dqcs::PredefinedGate::Z;
    }
    if (type == "h") {
      num_targets = 1;
      predef_gate = dqcs::PredefinedGate::H;
    }
    if (type == "s") {
      num_targets = 1;
      predef_gate = dqcs::PredefinedGate::S;
    }
    if (type == "sdag") {
      num_targets = 1;
      predef_gate = dqcs::PredefinedGate::S_DAG;
    }
    if (type == "t") {
      num_targets = 1;
      predef_gate = dqcs::PredefinedGate::T;
    }
    if (type == "tdag") {
      num_targets = 1;
      predef_gate = dqcs::PredefinedGate::T_DAG;
    }
    if (type == "x90") {
      num_targets = 1;
      predef_gate = dqcs::PredefinedGate::RX_90;
    }
    if (type == "mx90") {
      num_targets = 1;
      predef_gate = dqcs::PredefinedGate::RX_M90;
    }
    if (type == "y90") {
      num_targets = 1;
      predef_gate = dqcs::PredefinedGate::RY_90;
    }
    if (type == "my90") {
      num_targets = 1;
      predef_gate = dqcs::PredefinedGate::RY_M90;
    }
    if (type == "rx") {
      num_targets = 1;
      has_angle = true;
      predef_gate = dqcs::PredefinedGate::RX;
    }
    if (type == "ry") {
      num_targets = 1;
      has_angle = true;
      predef_gate = dqcs::PredefinedGate::RY;
    }
    if (type == "rz") {
      num_targets = 1;
      has_angle = true;
      predef_gate = dqcs::PredefinedGate::RZ;
    }
    if (type == "cr" || type == "crk") {
      num_targets = 2;
      has_angle = true;
      predef_gate = dqcs::PredefinedGate::Phase;
    }
    if (type == "swap") {
      num_targets = 2;
      predef_gate = dqcs::PredefinedGate::Swap;
    }
    if (type == "cnot") {
      num_targets = 2;
      predef_gate = dqcs::PredefinedGate::X;
    }
    if (type == "cz") {
      num_targets = 2;
      predef_gate = dqcs::PredefinedGate::Z;
    }
    if (type == "toffoli") {
      num_targets = 3;
      predef_gate = dqcs::PredefinedGate::X;
    }
    if (num_targets) {

      // Figure out the qubit indices to operate on.
      auto qubit_idxs = std::vector<std::vector<size_t>>();
      if (num_targets == 1) {
        qubit_idxs.push_back(operation.getQubitsInvolved().getSelectedQubits().getIndices());
      } else {
        for (int i = 1; i <= num_targets; i++) {
          qubit_idxs.push_back(operation.getQubitsInvolved(i).getSelectedQubits().getIndices());
        }
      }

      // Fail if the args have different sizes.
      for (int i = 1; i < num_targets; i++) {
        if (qubit_idxs[0].size() != qubit_idxs[i].size()) {
          throw std::runtime_error("Gate has differently-sized qubit argument ranges");
        }
      }

      // Execute the parallel gates.
      for (auto g = 0; g < qubit_idxs[0].size(); g++) {

        // Figure out the predefined gate parameters.
        dqcs::ArbData params = dqcs::ArbData();
        if (has_angle) {
          params.with_arg(operation.getRotationAngle());
        }

        // Construct the qubit set.
        dqcs::QubitSet qubit_refs = dqcs::QubitSet();
        for (int i = 0; i < num_targets; i++) {
          qubit_refs.push(qubits[qubit_idxs[i][g]]);
        }

        // Send the gate.
        state.gate(dqcs::Gate::predefined(predef_gate, std::move(qubit_refs), std::move(params)));

      }

      return true;
    }

    throw std::runtime_error("Unsupported gate: " + type);
  }

  /**
   * Run callback.
   */
  dqcs::ArbData run(
    dqcs::RunningPluginState &state,
    dqcs::ArbData &&args
  ) {
    // Read the file. FIXME: this leaks a handle if loading fails.
    FILE *fp = std::fopen(filename.c_str(), "r");
    compiler::QasmSemanticChecker sm(fp);
    std::fclose(fp);
    auto cqasm = sm.getQasmRepresentation();

    // Allocate qubits, bits, and measurements.
    qubits = state.allocate(cqasm.numQubits()).drain_into_vector();
    for (auto i = 0; i < cqasm.numQubits(); i++) {
      bits.push_back(false);
      measurements.emplace_back();
    }

    // Arb the error model downstream in case we're using QX.
    // TODO

    // Iterate over the subcircuits.
    for (auto circuit : cqasm.getSubCircuits().getAllSubCircuits()) {
      DQCSIM_INFO(
        "Running %d iterations for subcircuit %s...",
        circuit.numberIterations(),
        circuit.nameSubCircuit().c_str());
      for (int iter = 1; iter <= circuit.numberIterations(); iter++) {
        DQCSIM_DEBUG(
          "Running iteration %d of subcircuit %s...",
          iter,
          circuit.nameSubCircuit().c_str());
        for (auto bundle : circuit.getOperationsCluster()) {
          bool timed = false;
          for (auto op : bundle->getOperations()) {
            timed |= exec_gate(state, *op);
          }
          if (timed) {
            state.advance(1);
          }
        }
      }
    }

    // Construct the JSON return value.
    auto retval = "{\"qubits\": []}"_json;
    for (size_t i = 0; i < measurements.size(); i++) {
      nlohmann::json qubit = measurements[i].latest_json();
      qubit["value"] = (int)bits[i];
      if (measurements[i].num_samples()) {
        qubit["average"] = measurements[i].p1();
      }
      retval["qubits"].push_back(qubit);
    }

    // Free the downstream qubits when we're done, so we don't leak anything.
    for (auto qubit : qubits) {
      state.free(qubit);
    }
    qubits.clear();
    bits.clear();
    measurements.clear();

    return dqcs::ArbData().with_json(retval);
  }

};

int main(int argc, char *argv[]) {

//   // Pop the first argument, which is the path to the cQASM file.
  if (argc != 3) {
    std::cerr << "Expected two command-line arguments. Apply this plugin to a .cq file!" << std::endl;
  }
  std::string filename = std::string(argv[1]);
  argv[1] = argv[2];
  argc--;

  // Run the plugin.
  CQASMPlugin cQASMPlugin(std::move(filename));
  return dqcs::Plugin::Frontend("cQASM", "JvS", "0.0.1")
    .with_run(&cQASMPlugin, &CQASMPlugin::run)
    .run(argc, argv);

}
