using System.ComponentModel.DataAnnotations;
using Microsoft.AspNetCore.Identity;
using Microsoft.AspNetCore.Mvc;
using Microsoft.AspNetCore.Mvc.RazorPages;
using Microsoft.AspNetCore.RateLimiting;

namespace QuartusDebugger.Web.Pages;

[EnableRateLimiting("accounts")]
public sealed class LoginModel(SignInManager<StudentAccount> signIn) : PageModel
{
    [BindProperty, Required, RegularExpression(@"^[0-9]{8}$", ErrorMessage = "Student ID must contain exactly eight digits.")]
    public string StudentId { get; set; } = "";
    [BindProperty, Required, StringLength(128)]
    public string Password { get; set; } = "";
    public IActionResult OnGet() => User.Identity?.IsAuthenticated == true ? RedirectToPage("/Index") : Page();
    public async Task<IActionResult> OnPostAsync()
    {
        if (!ModelState.IsValid) return Page();
        var result = await signIn.PasswordSignInAsync(StudentId, Password, false, lockoutOnFailure: true);
        if (result.Succeeded) return RedirectToPage("/Index");
        ModelState.AddModelError("", "Unable to sign in. Check your credentials or try again later.");
        return Page();
    }
}
