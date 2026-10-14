import math
import random
import unittest

import sprt

ELO0 = 0.0
ELO1 = 20.0
SIMULATION_TRIALS = 200
SIMULATION_MAX_PAIRS = 3000
MAX_ERROR_RATE = 0.08


def regularized_probabilities(counts):
    regularized = [n + 1e-3 for n in counts]
    total = sum(regularized)
    return [n / total for n in regularized]


def simulate(rng, true_elo):
    win = sprt.expected_score(true_elo)
    loss = 1.0 - win
    weights = [loss * loss, 0.0, 2.0 * win * loss, 0.0, win * win]
    counts = [0] * 5
    for _ in range(SIMULATION_MAX_PAIRS):
        counts[rng.choices(range(5), weights)[0]] += 1
        decision = sprt.decide(sprt.llr(counts, ELO0, ELO1))
        if decision is not None:
            return decision
    return None


class ExpectedScoreTest(unittest.TestCase):
    def test_zero_elo_is_half(self):
        self.assertAlmostEqual(sprt.expected_score(0.0), 0.5)

    def test_elo_round_trip(self):
        for elo in (-200.0, -20.0, 0.0, 20.0, 200.0):
            self.assertAlmostEqual(sprt.elo_from_score(sprt.expected_score(elo)), elo, places=9)


class PairIndexTest(unittest.TestCase):
    def test_indices(self):
        self.assertEqual(sprt.pair_index(2, 0, 0), 4)
        self.assertEqual(sprt.pair_index(1, 0, 1), 3)
        self.assertEqual(sprt.pair_index(1, 1, 0), 2)
        self.assertEqual(sprt.pair_index(0, 0, 2), 2)
        self.assertEqual(sprt.pair_index(0, 1, 1), 1)
        self.assertEqual(sprt.pair_index(0, 2, 0), 0)

    def test_rejects_other_game_counts(self):
        with self.assertRaises(ValueError):
            sprt.pair_index(2, 1, 0)
        with self.assertRaises(ValueError):
            sprt.pair_index(1, 0, 0)


class MleDistributionTest(unittest.TestCase):
    def test_sum_and_mean(self):
        for counts in ([0, 0, 0, 0, 1], [3, 1, 10, 2, 5], [0, 0, 0, 0, 7], [7, 0, 0, 0, 0]):
            probabilities = regularized_probabilities(counts)
            for elo in (ELO0, ELO1):
                score = sprt.expected_score(elo)
                distribution = sprt.mle_distribution(probabilities, score)
                self.assertAlmostEqual(sum(distribution), 1.0, places=9)
                self.assertAlmostEqual(sum(p * x for p, x in zip(distribution, sprt.PAIR_SCORES)), score, places=9)
                self.assertTrue(all(p > 0.0 for p in distribution))


class LlrTest(unittest.TestCase):
    def test_one_winning_pair_does_not_decide(self):
        value = sprt.llr([0, 0, 0, 0, 1], ELO0, ELO1)
        self.assertAlmostEqual(value, 0.0559, places=3)
        self.assertIsNone(sprt.decide(value))

    def test_no_pairs_is_zero(self):
        self.assertEqual(sprt.llr([0, 0, 0, 0, 0], ELO0, ELO1), 0.0)

    def test_one_sided_counts_are_finite_and_decide(self):
        wins = sprt.llr([0, 0, 0, 0, 500], ELO0, ELO1)
        losses = sprt.llr([500, 0, 0, 0, 0], ELO0, ELO1)
        self.assertTrue(math.isfinite(wins))
        self.assertTrue(math.isfinite(losses))
        self.assertEqual(sprt.decide(wins), "H1")
        self.assertEqual(sprt.decide(losses), "H0")

    def test_even_counts_favor_h0(self):
        self.assertLess(sprt.llr([100, 0, 200, 0, 100], ELO0, ELO1), 0.0)

    def test_extreme_bounds_are_finite(self):
        for elo0, elo1 in ((-1000.0, 1000.0), (990.0, 1000.0), (-1000.0, -990.0)):
            for counts in ([0, 0, 0, 0, 5], [5, 0, 0, 0, 0], [3, 1, 10, 2, 5]):
                self.assertTrue(math.isfinite(sprt.llr(counts, elo0, elo1)))


class DecideTest(unittest.TestCase):
    def test_bounds(self):
        lower, upper = sprt.bounds()
        self.assertAlmostEqual(lower, -2.9444, places=4)
        self.assertAlmostEqual(upper, 2.9444, places=4)

    def test_decisions_around_bounds(self):
        self.assertEqual(sprt.decide(2.95), "H1")
        self.assertIsNone(sprt.decide(2.94))
        self.assertIsNone(sprt.decide(-2.94))
        self.assertEqual(sprt.decide(-2.95), "H0")


class EloEstimateTest(unittest.TestCase):
    def test_no_pairs(self):
        self.assertIsNone(sprt.elo_estimate([0, 0, 0, 0, 0]))

    def test_even_counts_are_zero(self):
        elo, low, high = sprt.elo_estimate([10, 0, 20, 0, 10])
        self.assertAlmostEqual(elo, 0.0)
        self.assertAlmostEqual(low, -high)
        self.assertLess(low, 0.0)

    def test_hand_computed_example(self):
        elo, low, high = sprt.elo_estimate([1, 2, 3, 2, 2])
        self.assertAlmostEqual(elo, 34.86007, places=4)
        self.assertAlmostEqual(low, -102.62173, places=4)
        self.assertAlmostEqual(high, 184.90886, places=4)

    def test_all_wins_is_finite(self):
        self.assertTrue(all(math.isfinite(value) for value in sprt.elo_estimate([0, 0, 0, 0, 5])))


class SimulationTest(unittest.TestCase):
    def test_error_rates(self):
        rng = random.Random(1)
        false_h1 = sum(simulate(rng, ELO0) == "H1" for _ in range(SIMULATION_TRIALS)) / SIMULATION_TRIALS
        false_h0 = sum(simulate(rng, ELO1) == "H0" for _ in range(SIMULATION_TRIALS)) / SIMULATION_TRIALS
        self.assertLessEqual(false_h1, MAX_ERROR_RATE)
        self.assertLessEqual(false_h0, MAX_ERROR_RATE)


if __name__ == "__main__":
    unittest.main()
