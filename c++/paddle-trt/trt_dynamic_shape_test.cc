#include <chrono>
#include <iostream>
#include <memory>
#include <numeric>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include "paddle/include/paddle_inference_api.h"

using paddle_infer::Config;
using paddle_infer::Predictor;
using paddle_infer::CreatePredictor;
using paddle_infer::PrecisionType;

DEFINE_string(model_file, "", "Path of the inference model file.");
DEFINE_string(params_file, "", "Path of the inference params file.");
DEFINE_string(model_dir, "", "Directory of the inference model.");
DEFINE_int32(batch_size, 1, "Batch size.");

using Time = decltype(std::chrono::high_resolution_clock::now());
Time time() { return std::chrono::high_resolution_clock::now(); };
double time_diff(Time t1, Time t2) {
  typedef std::chrono::microseconds ms;
  auto diff = t2 - t1;
  ms counter = std::chrono::duration_cast<ms>(diff);
  return counter.count() / 1000.0;
}

std::shared_ptr<Predictor> InitPredictor() {
  Config config;
  if (FLAGS_model_dir != "") {
    config.SetModel(FLAGS_model_dir);
  } else {
    config.SetModel(FLAGS_model_file, FLAGS_params_file);
  }
  config.EnableUseGpu(500, 0);
  config.EnableTensorRtEngine(1 << 30, FLAGS_batch_size, 5,
                              PrecisionType::kFloat32, false, false);
  std::map<std::string, std::vector<int>> min_input_shape = {
      {"image", {FLAGS_batch_size, 3, 112, 112}}};
  std::map<std::string, std::vector<int>> max_input_shape = {
      {"image", {FLAGS_batch_size, 3, 448, 448}}};
  std::map<std::string, std::vector<int>> opt_input_shape = {
      {"image", {FLAGS_batch_size, 3, 224, 224}}};
  config.SetTRTDynamicShapeInfo(min_input_shape, max_input_shape,
                                opt_input_shape);
  return CreatePredictor(config);
}

void run(Predictor *predictor, const std::vector<float> &input,
         const std::vector<int> &input_shape, std::vector<float> *out_data) {
  int input_num = std::accumulate(input_shape.begin(), input_shape.end(), 1,
                                  std::multiplies<int>());

  auto input_names = predictor->GetInputNames();
  auto input_t = predictor->GetInputHandle(input_names[0]);
  input_t->Reshape(input_shape);
  input_t->CopyFromCpu(input.data());

  CHECK(predictor->Run());

  auto output_names = predictor->GetOutputNames();
  // there is only one output of Resnet50
  auto output_t = predictor->GetOutputHandle(output_names[0]);
  std::vector<int> output_shape = output_t->shape();
  int out_num = std::accumulate(output_shape.begin(), output_shape.end(), 1,
                                std::multiplies<int>());

  out_data->resize(out_num);
  output_t->CopyToCpu(out_data->data());
}
 
int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  auto predictor = InitPredictor();
  std::vector<int> input_shape = {FLAGS_batch_size, 3, 224, 224};
  // Init input as 1.0 here for example. You can also load preprocessed real
  // pictures to vectors as input.
  std::vector<float> input_data(FLAGS_batch_size * 3 * 224 * 224, 1.0);
  std::vector<float> out_data;
  run(predictor.get(), input_data, input_shape, &out_data);
  // Print first 20 outputs
  for (int i = 0; i < 20; i++) {
    LOG(INFO) << out_data[i] << std::endl;
  }
  return 0;
}
