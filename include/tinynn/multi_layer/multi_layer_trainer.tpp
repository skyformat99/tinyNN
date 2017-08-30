#include <algorithm>
#include <iostream>
#include <cmath>
#include <cassert>

#include "../activation.hpp"
#include "multi_layer_trainer.hpp"
#include "trainable.hpp"

namespace tnn
{
    template<typename T, size_t InSize>
    multi_layer_trainer<T, InSize>::
    multi_layer_trainer(
        std::vector<size_t> const& layer_setting,
        double eta,
        uint64_t max_iterations,
        std::function<double(double)> const& activation_fun,
        std::function<double(double)> const& activation_fun_derived,
        bool verbose,
        bool history) :_eta(eta),
                       _max_iterations(max_iterations),
                       _trainee(nullptr),
                       _history(),
                       _distr(),
                       _verbose(verbose),
                       _save_history(history),
                       _activation_func(activation_fun),
                       _activation_func_derived(activation_fun_derived),
                       _layer_setting(layer_setting)
    {
        std::random_device seed_gen;
        _random_engine.seed(seed_gen());

        _history.reserve(max_iterations);
    }

    
    template<typename T, size_t InSize>
    typename multi_layer_trainer<T, InSize>::separated_data_set
    multi_layer_trainer<T, InSize>::
    separate_in_out(matrix_dyn<T> const& train_data) const
    {
        auto separated = std::vector<separated_data_set>();
        auto cols = separated.columns();
        auto rows = separated.rows();

        separated.reserve(rows);

        for(auto i = 0u; i < rows; ++i)
        {
            auto in = vector<T, InSize>();
            auto out = vector_dyn<T>(cols - InSize - 1);
            for(auto j = 0u; j < InSize + 1; ++j)
                in[j] = train_data(i, j);

            for(auto j = InSize + 1; j < cols; ++j)
                out[j - InSize - 1] = train_data(i, j);
        }

        return separated;
    }

    template<typename T, size_t InSize>
    std::tuple<vector<T, InSize>, vector_dyn<T>>
    multi_layer_trainer<T, InSize>::
    pick_random_data(std::vector<separated_data_set> const& set) 
    {
        auto idx = _distr(_random_engine);
        return set[idx];
    }

    template<typename T, size_t InSize>
    double
    multi_layer_trainer<T, InSize>::
    accuracy_percent(
        multi_layer_perceptron<T, InSize> const& perceptron,
        matrix_dyn<T> const& train_data) const
    {
        auto rows = train_data.rows();
        auto cols = train_data.columns(); 
        auto input = matrix_dyn<T>(rows, InSize + 1);
        auto answer = matrix_dyn<T>(rows, cols - InSize - 1);

        for(auto i = 0; i < rows; ++i)
        {
            for(auto j = 0; j < InSize; ++j)
                input(i, j) = train_data(i, j);
            for(auto j = InSize; j < cols; ++j)
                answer(i, j) = train_data(i, j);
        }
            
        auto result = perceptron(input);
        auto error_vec = vector_dyn<T>(answer- answer);

        auto abs_error_vec = map(
            error_vec,
            [](T elem){ return std::abs(elem); });

        return std::accumulate(abs_error_vec.begin(),
                               abs_error_vec.end(), 0.0)
            / abs_error_vec.size() * 100.0;
    }

    template<typename T, size_t InSize>
    vector_dyn<T>
    multi_layer_trainer<T, InSize>::
    forward_layer(size_t layer_num,
                  vector<T, InSize> const& input) const
    {
        auto predicted_result = (*_trainee)(layer_num, input);
        return predicted_result;
    }

    template<typename T, size_t InSize>
    void
    multi_layer_trainer<T, InSize>::
    assert_train_data(matrix_dyn<T> const& train_data)
    {
        // auto rows = train_data.rows();
        // auto cols = train_data.columns();
        // (void)rows;

        // assert(cols < InSize + 2 &&
        //        "input dimension too small: the dimension should be InSize + 1(bias) + 1(answer).");
    }

    template<typename T, size_t InSize>
    multi_layer_trainer<T, InSize>::history::
    history(vector<T, InSize>&& input_,
            vector_dyn<T>&& answer_,
            vector_dyn<T>&& output_,
            T error_)
        : input(std::move(input_)),
          answer(std::move(answer_)),
          output(std::move(output_)),
          error(error_)
    {}

    template<typename T, size_t InSize>
    typename multi_layer_trainer<T, InSize>::history
    multi_layer_trainer<T, InSize>::
    make_history(vector<T, InSize>&& input,
                 vector_dyn<T>&& output,
                 vector_dyn<T>&& answer,
                  T error) const
    {
        auto container =
            typename multi_layer_trainer<T, InSize>::history(
                std::move(input), std::move(answer),
                std::move(output), error);

        return container;
    }

    template<typename T, size_t InSize>
    std::vector<vector_dyn<T>>
    multi_layer_trainer<T, InSize>::
    forward_propagate(vector<T, InSize> const& train_data) const
    {
        auto result_by_layer = std::vector<vector<T, InSize>>();
        auto layer_num = _layer_setting.size();
        result_by_layer.reserve(layer_num);


        result_by_layer.push_back(
            _trainee->feed_layer(0, train_data));

        for(auto i = 1u; i < layer_num; ++i)
        {
            result_by_layer.push_back(
                _trainee->feed_layer(i, result_by_layer[i - 1]));
        }

        return result_by_layer;
    }



    template<typename T, size_t InSize>
    void
    multi_layer_trainer<T, InSize>::
    backward_propagate(
        std::vector<vector_dyn<T>> const& output_per_layer,
        vector_dyn<T> const& answer) 
    {
        auto output = output_per_layer.back();
        auto delta =
            (answer - output) * map(output,
                                    _activation_func_derived);

        auto layer_num = output.size();
        auto correction = eta * delta * map(output, _activation_func);
        _trainee->update_weight(layer_num - 1, correction);
        for(auto i = layer_num - 1; i >= 0u; ++i)
        {
            auto Fds = map(output_per_layer[i],
                           _activation_func_derived);
            delta = Fds * _trainee->feed_forward(delta);
            correction = eta * delta;
            _trainee->update_weight(layer_num - 1, correction);
        }
    }
    
    template<typename T, size_t InSize>
    double
    multi_layer_trainer<T, InSize>::
    calculate_error(vector_dyn<T> const& error_vector) const
    {
        return std::accumulate(
            error_vector.begin(),
            error_vector.end(), 0.0) / error_vector.size();
    }


    template<typename T, size_t InSize>
    multi_layer_perceptron<T, InSize>
    multi_layer_trainer<T, InSize>::
    train(matrix_dyn<T> const& train_data)
    {
        assert_train_data(train_data);

        auto train_data_separated = separate_in_out(train_data);

        _distr.param(
            std::uniform_int_distribution<uint32_t>::
            param_type(0, train_data.rows() - 1));

        auto _perceptron
            = std::make_unique<multi_layer_perceptron<T, InSize>>(
                _layer_setting,
                _activation_func);

        _trainee = _perceptron.get();

        if(_verbose)
            std::cout << "  iteration" << "  error" << std::endl;

        for(size_t it = 0; it < _max_iterations; ++it)
        {
            auto [train_input, train_answer]
                = pick_random_data(train_data_separated);

            auto result_by_layer = forward_propagate(train_input);
            auto error = train_answer - result_by_layer.back();
            auto avg_error = calculate_error(error);
            backward_propagate(result_by_layer, train_answer);
            
            if(_save_history)
            {
                _history.push_back(
                    make_history(std::move(train_input),
                                 std::move(result_by_layer.back()),
                                 std::move(train_answer), avg_error));
            }

            if(_verbose)
                std::cout << " - " << it+1 << "         "
                          << avg_error << std::endl;
        }
        
        if(_verbose)
            std::cout << "train over\n"
                      << "average error: "
                      << accuracy_percent(*_perceptron,
                                          train_data_separated)
                      << "%" << std::endl;

        return *_perceptron;
    }
}
