#include <string>
#include <cstdio>
#include <ios>
#include <dqcsim>
#include "qasm_semantic.hpp"

// Alias the relevant namespaces to something shorter.
namespace dqcs = dqcsim::wrap;
namespace cq = compiler;

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
   * Constructor.
   */
  CQASMPlugin(std::string &&filename) : filename(filename) {
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

    // Allocate qubits downstream.
    auto qubits = state.allocate(cqasm.numQubits()).drain_into_vector();

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
          size_t delay = 1;
          for (auto op : bundle->getOperations()) {
            // TODO
            DQCSIM_TRACE("gate: %s", op->getType().c_str());
          }
          state.advance(delay);
        }
      }
    }

    // Free the downstream qubits when we're done, so we don't leak anything.
    for (auto qubit : qubits) {
      state.free(qubit);
    }

    return args;
  }

};

int main(int argc, char *argv[]) {

  // Pop the first argument, which is the path to the cQASM file.
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
