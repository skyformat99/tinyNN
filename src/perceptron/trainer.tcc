#include <algorithm>
#include <functional>
#include <iostream>
#include <cmath>
#include <cassert>

#include "trainer.hpp"
#include "trainable.hpp"

namespace tnn
{
    template<typename T, size_t InSize>
    trainer<T, InSize>::
    trainer(double eta,
            uint64_t max_iterations,
            std::function<double(double)> const& activation_fun,
            bool verbose)
        :_eta(eta),
         _max_iterations(max_iterations),
         _trainee(nullptr),
         _error(),
         _distr(),
         _verbose(verbose),
         _activation_func(activation_fun)
    {
        std::random_device seed_gen;
        _random_engine.seed(seed_gen());

        _error.reserve(max_iterations);
    }

    
    template<typename T, size_t InSize>
    typename trainer<T, InSize>::separated_data_set
    trainer<T, InSize>::
    separate_in_out(matrix_dyn<T> const& train_data) const
    {
        auto rows = train_data.rows();
        auto cols = train_data.columns();

        auto input = matrix_dyn<T>(rows, cols - 1);
        auto output = vector_dyn<T>(rows);

        for(auto i = 0u; i < rows; ++i)
        {
            for(auto j = 0u; j < cols- 1; ++j)
                input(i, j) = train_data(i, j);
            output[i] = train_data(i, cols - 1);
        }

        return {input, output};
    }

    template<typename T, size_t InSize>
    std::tuple<vector<T, InSize>, T>
    trainer<T, InSize>::
    pick_random_data(separated_data_set const& set) 
    {
        vector<T, InSize> row;
        auto idx = _distr(_random_engine);

        auto in = std::get<0>(set);
        auto out = std::get<1>(set);

        for(auto it = 0u; it < InSize; ++it)
            row[it] = in(idx, it);

        return {row, out[idx]};
    }

    template<typename T, size_t InSize>
    double
    trainer<T, InSize>::
    accuracy_percent(perceptron<T, InSize> const& perceptron,
                     separated_data_set const& train_data) const
    {
        auto result = perceptron(std::get<0>(train_data));
        auto correct = std::get<1>(train_data);
        auto error_vec = vector_dyn<T>(correct - result);

        auto abs_error_vec = map(
            error_vec,
            [](T elem){ return std::abs(elem); });

        return std::accumulate(abs_error_vec.begin(),
                               abs_error_vec.end(), 0.0)
            / abs_error_vec.size() * 100.0;
    }

    template<typename T, size_t InSize>
    T
    trainer<T, InSize>::
    predict(vector<T, InSize> const& input) const
    {
        T predicted_result = (*_trainee)(input);
        return predicted_result;
    }

    template<typename T, size_t InSize>
    void
    trainer<T, InSize>::
    assert_train_data(matrix_dyn<T> const& train_data)
    {
        auto rows = train_data.rows();
        auto cols = train_data.columns();
        (void)rows;

        assert(cols < InSize + 2 &&
               "input dimension too small: the dimension should be InSize + 1(bias) + 1(answer).");
    }

    template<typename T, size_t InSize>
    perceptron<T, InSize>
    trainer<T, InSize>::
    train(matrix_dyn<T> const& train_data)
    {
        assert_train_data(train_data);

        auto train_data_separated = separate_in_out(train_data);

        _distr.param(
            std::uniform_int_distribution<uint32_t>::
            param_type(0, train_data.rows() - 1));

        auto _perceptron
            = std::make_unique<perceptron<T, InSize>>(
                _activation_func);

        _trainee = _perceptron.get();

        if(_verbose)
            std::cout << "  iteration" << "  error" << std::endl;

        for(size_t it = 0; it < _max_iterations; ++it)
        {
            auto [train_input, train_answer]
                = pick_random_data(train_data_separated);

            T result = predict(train_input);
            T error = train_answer - result;

            _error.push_back(error);
            auto correction = _eta * error * train_input;
            _trainee->update_weight(correction);

            if(_verbose)
                std::cout << " - " << it+1 << "         "
                          << _error[it] << std::endl;
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
