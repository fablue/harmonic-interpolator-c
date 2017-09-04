#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>


#define MS_TO_NANO_MULTIPLIER  1000000

/**
 * The params which define the harmonic interpolator.
 * Those map to the following function
 *
 *   x(t) = 1 * exp(-gamma * t) * cos (omega * t)
 *
 * Which represents a "stepped harmonic oscillator"
 *
 */
struct interpolator_params {
    double omega;
    double gamma;
};


/**
 * The interpolator settings represent a different parametrization of the stepped harmonic oscillator
 * which lead to very easy usage in mobile animations where you need certain constraints
 *
 * 1. x(0) = 0
 * 2. x(1) = 1
 *
 * 3. Easy control of the look and feel of your animation (which is given here by overshoot and rest_position_runs)
 */
struct interpolator_settings {
    /**
     * The maximal (normalized) amount this interpolator overshoots:
     *  x(t_max_x) = 1 * overshoot
     */
    double overshoot;

    /**
     * The number of "how often the oscillator crosses the value 1". The final state is not counted!
     */
    double rest_position_runs;
};

typedef struct interpolator_settings InterpolatorSettings;
typedef struct interpolator_params InterpolatorParams;



/**
 *  Function 1 - exp(-gamma * time) * cos(omega*time)
 *
 * @param omega The frequency of the oscillator
 * @param gamma The damping of the oscillator
 * @param time The time (expected a value between 0 and 1)
 * @return A value x. x(0) = 0    x(1) = 1
 */
double calculate_interpolation_raw(double omega, double gamma, double time) {
    return 1 - exp(-gamma * time) * cos(omega * time);

}


/**
 * Same as @see calculate_interpolation_raw
 */
double calculate_interpolation(InterpolatorParams params, double time) {
    return calculate_interpolation_raw(params.omega, params.gamma, time);
}


/**
 * Calculates the frequency of the oscillator to the constraint of x(1) = 1
 */
double calculate_omega(InterpolatorSettings settings) {
    double rest_position_runs = settings.rest_position_runs;

    //We think of the oscillator being at 0.25 state (full deflection)
    double full_oscillations = rest_position_runs / 2 + 0.75;
    return 2 * M_PI * full_oscillations;
}

/**
 * Calculates the t_max_x (The time for extrem x)
 *
 * This is equivalent to the following problem:
 *
 *  d/dt (1 - exp(-gamma * t) * cos(omega * t ) = 0
 *
 * @param omega The frequency of the oscillator
 * @param gamma  The damping of the oscillator
 * @return The time where the oscillator is expected to have maximum overshoot
 */
double calculate_turning_time(double omega, double gamma) {
    double turning_time =
            2 * atan(omega / gamma - sqrt(pow(gamma, 2) + pow(omega, 2)) / gamma) / omega + M_PI / omega;
    return turning_time;
}


/**
 * Tries to find gamma with a certain precision
 * @param settings  The settings of the oscillator
 * @param omega  The already calculated omega for the parametrization
 * @return A 'good enough' gamma which leads to the expected overshoot
 */
double calculate_gamma(InterpolatorSettings settings, double omega) {

    /*
     * The precision of the gamma itself. NOT the precision of the deviation from the expected overshoot!
     */
    double precision = 0.01;

    /*
     * Start by estimating a t_max which should maximize our x(t)
     */
    double time = M_PI / omega;

    /**
     * Calculate a naive gamma for our expected time
     */
    double gamma = -log(settings.overshoot) / time;

    /**
     * Adjust our t_max by the newly calculated gamma
     */
    double adjusted_time = calculate_turning_time(omega, gamma);

    /**
     * Calculate the deviation from our expected overshoot
     */
    double interpolation = calculate_interpolation_raw(omega, gamma, adjusted_time) - 1;
    double deviation = fabs(settings.overshoot - interpolation);


    /**
     * Determine whether our naively calculated gamma was to high or to low.
     * If our interpolation is higher than the wanted overshoot, than we should increase
     * the damping. Else decrease
     */
    double sign;
    if (interpolation - settings.overshoot > 0) sign = 1;
    else sign = -1;


    /**
     * Now lets optimize our gamma by finding the lowest deviation, while finetuning!
     */
    while (true) {
        double tuned_gamma = gamma + sign * precision;
        double tuned_time = calculate_turning_time(omega, tuned_gamma);
        double tuned_interpolation = calculate_interpolation_raw(omega, tuned_gamma, tuned_time) - 1;
        double tuned_deviation = fabs(settings.overshoot - tuned_interpolation);
        if (tuned_deviation < deviation) {
            deviation = tuned_deviation;
            gamma = tuned_gamma;
        } else break;
    }


    return gamma;
}


/**
 * Performs a param transformation from our easy to handle "InterpolatorSettings" to the "InterpolatorParams"
 */
InterpolatorParams calculate_params(InterpolatorSettings settings) {
    double omega = calculate_omega(settings);
    double gamma = calculate_gamma(settings, omega);

    InterpolatorParams params = {
            .omega = omega,
            .gamma = gamma
    };

    return params;
}

/**
 * Test function of or our algorithm.
 * Tests the behaviour of the interpolator for a certain setting
 */
void test_interpolation() {
    InterpolatorSettings settings = {
            .rest_position_runs=4,
            .overshoot=0.2
    };

    InterpolatorParams params = calculate_params(settings);

    printf(""
                   "Testing interpolation settings\n"
                   "______________________________\n"
                   "rp_runs   : %lf\n"
                   "overshoot : %lf\n"
                   "omega     : %lf\n"
                   "gamma     : %lf\n"
                   "______________________________\n", settings.rest_position_runs, settings.overshoot, params.omega,
           params.gamma);


    double steps = 100;
    double test_precision = 0.01f;
    double last_interpolation = -1;
    int rest_position_counter = 0;
    double detected_overshoot = 0;

    for (__int32_t i = 0; i < steps; i++) {
        double time = i / steps;
        double interpolation = calculate_interpolation(params, time);
        double normalized_interpolation = interpolation - 1;

        printf("Time %lf -> %lf\n", time, interpolation);

        // If the sign changed from last_interpolation to interpolation, then one of both has to be
        // negative and the other positive, which results in a negative product
        if ((last_interpolation) * (normalized_interpolation) < 0) {
            rest_position_counter++;
        }

        // Since the first part of the oscillation starts at -1: Do not track until we went through
        // the rest position at least one time!
        if (rest_position_counter > 0 && fabs(detected_overshoot) < fabs(normalized_interpolation)) {
            detected_overshoot = normalized_interpolation;
        }


        last_interpolation = normalized_interpolation;
    }


    bool failure_flag = false;


    if (rest_position_counter - 1 != settings.rest_position_runs) {
        printf("Test failed. Rest position run should have been %lf but was %d\n",
               settings.rest_position_runs, rest_position_counter);

        failure_flag = true;
    }

    if (fabs(settings.overshoot - detected_overshoot) > test_precision) {
        printf("Test failed. Overshoot should have been %lf but was %lf\n",
               settings.overshoot, detected_overshoot);

        failure_flag = true;
    }


    if (!failure_flag) {
        printf("Test succeeded. Overshoot accuracy was %lf\n", fabs(settings.overshoot - detected_overshoot));
    }

}


/**
 * Will create a rather chunky command line animation to visualize the interpolator
 * @param settings The interpolator's settings
 * @param duration  How long should the animation run?
 * @param running_mode Should it print a new line every time, or remove the old one?
 */
void visualize_interpolation_cli(InterpolatorSettings settings, double duration, bool running_mode) {

    int max_points = 150;


    InterpolatorParams params = calculate_params(settings);

    double ms_running = 0;

    struct timespec interval;
    interval.tv_nsec = MS_TO_NANO_MULTIPLIER * 32;
    interval.tv_sec = 0;


    while (ms_running < duration) {

        double normalized = ms_running / duration;
        double interpolated = calculate_interpolation(params, normalized);

        long points = lround(max_points * interpolated);


        for (int i = 0; i < points; i++) {
            printf("#");

        }

        if (running_mode) printf("\n");
        else printf("\r");


        fflush(stdout);

        nanosleep(&interval, NULL);
        ms_running += interval.tv_nsec / MS_TO_NANO_MULTIPLIER;
    }
}


/**
 * Asks the user for visualization params and performs the visualisation
 */
void custom_visualization(){
    double rest_position_runs =0;
    double overshoot=0;
    long duration= 0;

    printf("How often should the interpolator cross the rest position?"); scanf("%lf", &rest_position_runs);
    printf("How far should the interpolator 'overshoot'?");               scanf("%lf", &overshoot);
    printf("How long should the animation run? (in ms)");                 scanf("%ld", &duration);

    InterpolatorSettings settings = {
            .rest_position_runs = rest_position_runs,
            .overshoot = overshoot
    };


    visualize_interpolation_cli(settings, duration, true);

}

int main() {
    test_interpolation();


    InterpolatorSettings long_running_settings = {
            .rest_position_runs = 16,
            .overshoot=0.85
    };

    InterpolatorSettings typical_mobile_animation = {
            .rest_position_runs = 4,
            .overshoot = 0.25
    };


    printf(""
                   "\n\n"
                   "################ CLI MENU #################\n"
                   "Press 'l' to run the long visualization\n"
                   "Press 'm' to run a typical mobile animation visualization\n"
                   "Press 'c' to enter custom params vor the visualization\n"
                   "Press any other key to exit\n");


    while (true) {
        char input;
        scanf("%c", &input);

        switch (input) {
            case '\n':
                break;
            case 'l':
                visualize_interpolation_cli(long_running_settings, 20000, true);
                break;

            case 'm':
                visualize_interpolation_cli(typical_mobile_animation, 2000, true);
                break;

            case 'c':
                custom_visualization();
                break;

            default:
                _exit(0);
        }


    }

}

